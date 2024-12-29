static const char *Net_Label(AppState *app)
{
    return app->net.is_server ? "SERVER" : "CLIENT";
}

static Uint8 *Net_PayloadAlloc(AppState *app, Uint32 size)
{
    Uint8 *result = app->net.payload_buf + app->net.payload_buf_used;
    {
        // return ptr to start of the buffer
        // on overflow
        Uint8 *end = (app->net.payload_buf + sizeof(app->net.payload_buf));
        if (end < (result + size))
        {
            Assert(false);
            result = app->net.payload_buf;
            app->net.send_err = true;
        }
    }
    app->net.payload_buf_used += size;
    return result;
}

static Uint8 *Net_PayloadMemcpy(AppState *app, void *data, Uint32 size)
{
    Uint8 *result = Net_PayloadAlloc(app, size);
    memcpy(result, data, size);
    return result;
}

static void Net_PayloadToPackets(AppState *app)
{
    // @todo(mg) add compression here

    SDL_zeroa(app->net.sender_packet_slices);
    bool packet_overflow = false;

    // calculate packet_count with ceil-rounding
    Uint32 needed_packet_count =
        (app->net.payload_buf_used + NET_MAX_PAYLOAD_SIZE - 1) / NET_MAX_PAYLOAD_SIZE;

    if (needed_packet_count > NET_MAX_PACKET_CHAIN_LENGTH)
    {
        packet_overflow = true;
    }

    Uint32 packet_index = 0;
    if (!packet_overflow)
    {
        S8 payload = S8_Make(app->net.payload_buf, app->net.payload_buf_used);

        while (payload.size)
        {
            // select index, bound check, advance count
            if (packet_index >= ArrayCount(app->net.sender_packet_slices))
            {
                packet_overflow = true;
                break;
            }

            // get payload, calc hash
            S8 payload_chunk = S8_PrefixConsume(&payload, NET_MAX_PAYLOAD_SIZE);
            Uint32 payload_hash = S8_Hash(0, payload_chunk);

            // header calc
            Net_PacketHeader header = {0};
            header.magic_value = NET_MAGIC_VALUE;
            header.packet_count = needed_packet_count;
            header.packet_id = packet_index;
            header.packet_payload_hash = (Uint32)payload_hash;
            header.tick_id = app->tick_id;


            S8 full_packet_slice = S8_Make(app->net.sender_packets_buf + NET_MAX_PACKET_SIZE * packet_index,
                                           NET_MAX_PACKET_SIZE);

            Uint64 total_needed_size = payload_chunk.size + sizeof(header);

            // size checks
            if (full_packet_slice.size < total_needed_size)
            {
                packet_overflow = true;
                break;
            }

            // save slice
            app->net.sender_packet_slices[packet_index] = S8_Prefix(full_packet_slice, total_needed_size);

            // header copy
            memcpy(full_packet_slice.str, &header, sizeof(header));
            full_packet_slice = S8_Skip(full_packet_slice, sizeof(header));

            // payload copy
            memcpy(full_packet_slice.str, payload_chunk.str, payload_chunk.size);
            full_packet_slice = S8_Skip(full_packet_slice, payload_chunk.size);

            // advance
            packet_index += 1;
        }
    }

    if (packet_overflow)
    {
        SDL_Log("%s: Can't transform payload to packets. Error at packet %u / %u packets (max %u). Payload size: %u",
                Net_Label(app),
                packet_index,
                needed_packet_count,
                (Uint32)ArrayCount(app->net.sender_packet_slices),
                app->net.payload_buf_used);
    }
}

static void Net_SendS8(AppState *app, Net_User destination, S8 msg)
{
    Assert(msg.size < 1280); // that seems to be a realistic reasonable max size? https://gafferongames.com/post/packet_fragmentation_and_reassembly/

    bool send_res = SDLNet_SendDatagram(app->net.socket,
                                        destination.address,
                                        destination.port,
                                        msg.str, msg.size);

    if (!send_res)
    {
        NET_VERBOSE_LOG("%s: Sending buffer of size %lluB to %s:%d; %s",
                        Net_Label(app), msg.size,
                        SDLNet_GetAddressString(destination.address),
                        (int)destination.port,
                        send_res ? "success" : "fail");
    }

    //SDL_Delay(10);
}

static void Net_SendChain(AppState *app, Net_User destination)
{
    ForArray(i, app->net.sender_packet_slices)
    {
        S8 slice = app->net.sender_packet_slices[i];
        if (!slice.size)
            break;
        Net_SendS8(app, destination, slice);
    }

}

static void Net_SendPayloadAndFlush(AppState *app)
{
    Net_PayloadToPackets(app);

    if (app->net.is_server)
    {
        ForU32(i, app->net.user_count)
        {
            Net_SendChain(app, app->net.users[i]);
        }
    }
    else
    {
        Net_SendChain(app, app->net.server_user);
    }

    app->net.payload_buf_used = 0;
}

static bool Net_ConsumeS8(S8 *msg, void *dest, Uint64 size)
{
    Uint64 copy_size = Min(size, msg->size);
    bool err = copy_size != size;

    if (err)
        memset(dest, 0, size); // fill dest with zeroes

    memcpy(dest, msg->str, copy_size);
    *msg = S8_Skip(*msg, size);

    return err;
}

static bool Net_UserMatch(Net_User a, Net_User b)
{
    return (a.port == b.port &&
            SDLNet_CompareAddresses(a.address, b.address) == 0);
}

static bool Net_UserMatchAddrPort(Net_User a, SDLNet_Address *address, Uint16 port)
{
    return (a.port == port &&
            SDLNet_CompareAddresses(a.address, address) == 0);
}

static Net_User *Net_FindUser(AppState *app, SDLNet_Address *address, Uint16 port)
{
    ForU32(i, app->net.user_count)
    {
        if (Net_UserMatchAddrPort(app->net.users[i], address, port))
            return app->net.users + i;
    }
    return 0;
}

static Net_User *Net_AddUser(AppState *app, SDLNet_Address *address, Uint16 port)
{
    if (app->net.user_count < ArrayCount(app->net.users))
    {
        Net_User *user = app->net.users + app->net.user_count;
        app->net.user_count += 1;
        user->address = SDLNet_RefAddress(address);
        user->port = port;
        return user;
    }
    return 0;
}

static void Net_IterateSend(AppState *app)
{
    bool is_server = app->net.is_server;
    bool is_client = !app->net.is_server;
    if (app->net.err) return;

    if (is_server && !app->net.user_count)
    {
        return;
    }

    {
        // hacky temporary network activity rate-limitting
        static Uint64 last_timestamp = 0;
        if (app->frame_time < last_timestamp + 900)
            return;
        last_timestamp = app->frame_time;
    }

    // send network test
    if (1)
    {
        if (is_client)
        {
            Tick_Command cmd = {};
            cmd.tick_id = app->tick_id;
            cmd.kind = Tick_Cmd_NetworkTest;
            Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

            Tick_NetworkTest test;
            ForArray(i, test.numbers)
                test.numbers[i] = i + 1;

            Net_PayloadMemcpy(app, &test, sizeof(test));
            Net_SendPayloadAndFlush(app);
        }
    }

    if (1)
    {
        if (is_server)
        {
            Tick_Command cmd = {};
            cmd.tick_id = app->tick_id;
            cmd.kind = Tick_Cmd_ObjHistory;
            Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

            Uint64 state_index = app->netobj.next_tick % ArrayCount(app->netobj.states);

            Uint8 *payload_start = 0;
            Uint8 *payload_end = 0;

            // copy range (next..ArrayCount)
            {
                Uint64 start = state_index;
                Uint64 states_to_copy = ArrayCount(app->netobj.states) - start;
                Uint64 copy_size = states_to_copy*sizeof(Tick_NetworkObjState);
                payload_start = Net_PayloadMemcpy(app, app->netobj.states + start, copy_size);
                payload_end = payload_start + copy_size;
            }

            if (state_index > 0) // copy range [0..next)
            {
                Uint64 states_to_copy = state_index;
                Uint64 copy_size = states_to_copy*sizeof(Tick_NetworkObjState);
                payload_end = Net_PayloadMemcpy(app, app->netobj.states, copy_size);
                payload_end += copy_size;
            }

            S8 payload_objs = S8_Range(payload_start, payload_end);
            Uint64 hash = S8_Hash(0, payload_objs);
            Net_PayloadMemcpy(app, &hash, sizeof(hash));
        }

        if (is_client)
        {
            Tick_Command cmd = {};
            cmd.tick_id = app->tick_id;
            cmd.kind = Tick_Cmd_Ping;
            Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

            Tick_Ping ping = {0};
            Net_PayloadMemcpy(app, &ping, sizeof(ping));
        }

        Net_SendPayloadAndFlush(app);
    }
}

static void Net_ProcessReceivedMessage(AppState *app, S8 full_message)
{
    S8 msg = full_message;
    while (msg.size)
    {
        Tick_Command cmd;
        Net_ConsumeS8(&msg, &cmd, sizeof(cmd));

        if (cmd.kind == Tick_Cmd_Ping)
        {
            Tick_Ping ping = {0};
            Net_ConsumeS8(&msg, &ping, sizeof(ping));
        }
        else if (cmd.kind == Tick_Cmd_ObjHistory)
        {
            Tick_NetworkObjHistory history;
            Net_ConsumeS8(&msg, &history, sizeof(history));

            S8 states_string = S8_Make((Uint8 *)history.states, sizeof(history.states));
            Uint64 states_hash = S8_Hash(0, states_string);
            Assert(history.total_hash == states_hash);

            if (cmd.tick_id < app->netobj.latest_server_at_tick)
            {
                // the msg we recieved now is older than the last message
                // from the server
                // let's drop the message?
                SDL_Log("%s: Dropping message with old tick_id: %llu, already received tick_id: %llu",
                        Net_Label(app), cmd.tick_id, app->netobj.latest_server_at_tick);
                return;
            }

            Uint64 server_tick_bump = cmd.tick_id - app->netobj.latest_server_at_tick;
            app->netobj.latest_server_at_tick = cmd.tick_id;

            // adjusting tick playback delay
            {
                Uint64 bump_index = app->netobj.tick_bump_history_next %
                    ArrayCount(app->netobj.tick_bump_history);
                app->netobj.tick_bump_history_next += 1;

                app->netobj.tick_bump_history[bump_index] = server_tick_bump;

                Uint64 biggest_tick_bump = 0;
                ForArray(i, app->netobj.tick_bump_history)
                {
                    biggest_tick_bump = Max(app->netobj.tick_bump_history[i],
                                            biggest_tick_bump);
                }

                Uint64 target_tick_delta = biggest_tick_bump + biggest_tick_bump/8 + 1;
                Uint64 current_tick_delta = app->netobj.latest_server_at_tick -
                    app->netobj.next_tick;

                app->netobj.tick_bump_correction = 0;
                if (current_tick_delta > target_tick_delta)
                    app->netobj.tick_bump_correction = current_tick_delta - target_tick_delta;

                if (app->netobj.tick_bump_correction)
                {
                    NET_VERBOSE_LOG("%s DELAY_ADJUSTMENT: biggest: %llu; target: %llu; "
                                    "current: %llu, correction: %llu",
                                    Net_Label(app), biggest_tick_bump, target_tick_delta,
                                    current_tick_delta, app->netobj.tick_bump_correction);
                }
            }

            // truncate next_tick; server state is too ahead
            if (app->netobj.next_tick + ArrayCount(app->netobj.states) <=
                app->netobj.latest_server_at_tick)
            {
                app->netobj.next_tick =
                    app->netobj.latest_server_at_tick - ArrayCount(app->netobj.states) + 1;
            }


            Uint64 fill_index = app->netobj.latest_server_at_tick + 1;
            CircleBufferFill(sizeof(Tick_NetworkObjState),
                             app->netobj.states, ArrayCount(app->netobj.states),
                             &fill_index,
                             history.states, ArrayCount(history.states));
        }
        else if (cmd.kind == Tick_Cmd_NetworkTest)
        {
            Tick_NetworkTest test;
            Net_ConsumeS8(&msg, &test, sizeof(test));

            ForArray(i, test.numbers)
            {
                Uint32 loaded = test.numbers[i];
                Uint32 expected = i + 1;
                bool compare = loaded == expected;
                Assert(compare);
            }
        }
        else
        {
            SDL_Log("%s: Unsupported cmd kind: %d",
                    Net_Label(app), (int)cmd.kind);
            return;
        }
    }
}

static void Net_ReceivePacket(AppState *app, S8 msg)
{
    if (msg.size < sizeof(Net_PacketHeader))
    {
        NET_VERBOSE_LOG("%s: packet rejected - it's too small, size: %llu",
                        Net_Label(app), msg.size);
        return;
    }

    Net_PacketHeader header;
    Net_ConsumeS8(&msg, &header, sizeof(header));

    if (!msg.size)
    {
        NET_VERBOSE_LOG("%s: packet rejected - empty payload",
                        Net_Label(app));
        return;
    }

    if (header.magic_value != NET_MAGIC_VALUE)
    {
        NET_VERBOSE_LOG("%s: packet rejected - invalid magic value: %u; expected: %u",
                        Net_Label(app), (Uint32)header.magic_value, NET_MAGIC_VALUE);
        return;
    }
    if (header.packet_count > NET_MAX_PACKET_CHAIN_LENGTH)
    {
        NET_VERBOSE_LOG("%s: packet rejected - packet_count too large: %u, max: %u",
                        Net_Label(app), (Uint32)header.packet_count, NET_MAX_PACKET_CHAIN_LENGTH);
        return;
    }
    if (header.packet_id >= NET_MAX_PACKET_CHAIN_LENGTH)
    {
        NET_VERBOSE_LOG("%s: packet rejected - packet_id too large: %u, max: %u",
                        Net_Label(app), (Uint32)header.packet_id, NET_MAX_PACKET_CHAIN_LENGTH-1);
        return;
    }

    Uint32 best_chain_index = ~0u;
    {
        Uint64 lowest_tick = header.tick_id;
        ForArray(i, app->net.receiver_chains)
        {
            Uint64 chain_tick = app->net.receiver_chains[i].tick_id;

            if (chain_tick == header.tick_id)
            {
                best_chain_index = i;
                break;
            }

            if (lowest_tick > chain_tick)
            {
                best_chain_index = i;
                lowest_tick = chain_tick;
            }
        }
    }

    if (best_chain_index >= ArrayCount(app->net.receiver_chains))
    {
        NET_VERBOSE_LOG("%s: packet rejected - too old (tick id: %llu), "
                        "all chains are filled with newer packets",
                        Net_Label(app), header.tick_id);
        return;
    }

    // validate hash
    Uint64 hash64 = S8_Hash(0, msg);
    Uint32 hash32 = (Uint32)hash64;
    if (hash32 != header.packet_payload_hash)
    {
        NET_VERBOSE_LOG("%s: packet rejected - invalid hash: %u; calculated: %u",
                        Net_Label(app), header.packet_payload_hash, hash32);
        return;
    }

    Net_PacketChain *chain = app->net.receiver_chains + best_chain_index;
    // clear if tick_id doesnt match
    if (chain->tick_id != header.tick_id)
    {
        chain->tick_id = header.tick_id;
        chain->packet_count = header.packet_count;
        SDL_zeroa(chain->packet_sizes);
    }

    if (chain->packet_count == header.tick_id)
    {
        NET_VERBOSE_LOG("%s: packet rejected - packet count (%u) doesn't match previous chain packet count (%u)",
                        Net_Label(app), (Uint8)chain->packet_count, (Uint8)header.packet_count);
        return;
    }

    // store into packet_slot
    S8 packet_buf = Net_GetChainBuffer(chain, header.packet_id);

    if (header.packet_id + 1 < header.packet_count)
    {
        if (packet_buf.size != msg.size)
        {
            NET_VERBOSE_LOG("%s: packet (%u / %u) rejected - packet buffer (size: %ullB) "
                            "has to match msg (size: %ullB) exactly",
                            Net_Label(app), (Uint32)header.packet_id,
                            (Uint32)header.packet_count, packet_buf.size, msg.size);
            return;
        }
    }

    if (packet_buf.size < msg.size)
    {
        NET_VERBOSE_LOG("%s: packet rejected - packet buffer (size: %ullB) overflow; msg size: %ullB",
                        Net_Label(app), packet_buf.size, msg.size);
        return;
    }

    chain->packet_sizes[header.packet_id] = msg.size;
    memcpy(packet_buf.str, msg.str, msg.size);

    // log
    {
        NET_VERBOSE_LOG("%s: saved packet id: %u (count %u); tick_id: %llu",
                        Net_Label(app), (Uint32)header.packet_id,
                        (Uint32)header.packet_count,
                        header.tick_id);
    }


    // are all packets in the chain ready?
    bool is_chain_incomplete = false;
    Uint64 chain_packet_total_size = 0;
    ForU32(i, chain->packet_count)
    {
        if (chain->packet_sizes[i])
        {
            chain_packet_total_size += chain->packet_sizes[i];
        }
        else
        {
            is_chain_incomplete = true;
            break;
        }
    }

    if (!is_chain_incomplete)
    {
        {
            NET_VERBOSE_LOG("%s: completed packets (count: %u); tick_id: %llu",
                            Net_Label(app), (Uint32)header.packet_count,
                            header.tick_id);
        }

        S8 complete_payload = S8_Make(chain->packet_buf, chain_packet_total_size);
        Net_ProcessReceivedMessage(app, complete_payload);
    }
}

static void Net_IterateReceive(AppState *app)
{
    bool is_server = app->net.is_server;
    bool is_client = !app->net.is_server;
    if (app->net.err) return;

    {
        // hacky temporary network activity rate-limitting
        static Uint64 last_timestamp = 0;
        if (app->frame_time < last_timestamp + 0)
            return;
        last_timestamp = app->frame_time;
    }

    for (;;)
    {
        SDLNet_Datagram *dgram = 0;
        int receive = SDLNet_ReceiveDatagram(app->net.socket, &dgram);
        if (!receive) break;
        if (!dgram) break;

        NET_VERBOSE_LOG("%s: got %d-byte datagram from %s:%d",
                        Net_Label(app),
                        (int)dgram->buflen,
                        SDLNet_GetAddressString(dgram->addr),
                        (int)dgram->port);

        if (is_client)
        {
            if (!Net_UserMatchAddrPort(app->net.server_user, dgram->addr, dgram->port))
            {
                SDL_Log("%s: dgram rejected - received from non-server address %s:%d",
                        Net_Label(app),
                        SDLNet_GetAddressString(dgram->addr), (int)dgram->port);
                goto datagram_cleanup;
            }
        }

        // save user
        if (is_server)
        {
            if (!Net_FindUser(app, dgram->addr, dgram->port))
            {
                SDL_Log("%s: saving user with port: %d",
                        Net_Label(app), (int)dgram->port);
                Net_AddUser(app, dgram->addr, dgram->port);
            }
        }

        S8 msg = S8_Make(dgram->buf, dgram->buflen);
        Net_ReceivePacket(app, msg);

        datagram_cleanup:
        SDLNet_DestroyDatagram(dgram);
    }
}

static void Net_Init(AppState *app)
{
    bool is_server = app->net.is_server;
    bool is_client = !app->net.is_server;

    SDL_Log(is_server ? "Launching as server" : "Launching as client");

    if (is_client)
    {
        const char *hostname = "localhost";
        SDL_Log("%s: Resolving server hostname '%s' ...",
                Net_Label(app), hostname);
        app->net.server_user.address = SDLNet_ResolveHostname(hostname);
        app->net.server_user.port = NET_DEFAULT_SEVER_PORT;
        if (app->net.server_user.address)
        {
            if (SDLNet_WaitUntilResolved(app->net.server_user.address, -1) < 0)
            {
                SDLNet_UnrefAddress(app->net.server_user.address);
                app->net.server_user.address = 0;
            }
        }

        if (!app->net.server_user.address)
        {
            app->net.err = true;
            SDL_Log("%s: Failed to resolve server hostname '%s'",
                    Net_Label(app), hostname);
        }
    }

    Uint16 port = (app->net.is_server ? NET_DEFAULT_SEVER_PORT : 0);
    app->net.socket = SDLNet_CreateDatagramSocket(0, port);
    if (!app->net.socket)
    {
        app->net.err = true;
        SDL_Log("%s: Failed to create socket",
                Net_Label(app));
    }
    else
    {
        SDL_Log("%s: Created socket",
                Net_Label(app));

#if NET_SIMULATE_PACKETLOSS
        SDLNet_SimulateDatagramPacketLoss(app->net.socket, NET_SIMULATE_PACKETLOSS);
        SDL_Log("%s: Simulating packetloss: %d",
                Net_Label(app), NET_SIMULATE_PACKETLOSS);
#endif
    }
}

static void Net_Deinit()
{
    Assert(!"@todo");
}
