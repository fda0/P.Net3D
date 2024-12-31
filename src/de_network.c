static bool Net_IsServer(AppState *app)
{
    return app->net.is_server;
}

static bool Net_IsClient(AppState *app)
{
    return !app->net.is_server;
}

static const char *Net_Label(AppState *app)
{
    return app->net.is_server ? "SERVER" : "CLIENT";
}

static Uint8 *Net_PayloadAlloc(AppState *app, Uint32 size)
{
    Uint8 *result = app->net.packet_payload_buf + app->net.payload_used;
    {
        // return ptr to start of the buffer
        // on overflow
        Uint8 *end = (app->net.packet_payload_buf + sizeof(app->net.packet_payload_buf));
        if (end < (result + size))
        {
            Assert(false);
            result = app->net.packet_payload_buf;
            app->net.packet_err = true;
        }
    }
    app->net.payload_used += size;
    return result;
}

static Uint8 *Net_PayloadMemcpy(AppState *app, void *data, Uint32 size)
{
    Uint8 *result = Net_PayloadAlloc(app, size);
    memcpy(result, data, size);
    return result;
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
        LOG(LogFlags_NetSend,
            "%s: Sending buffer of size %lluB to %s:%d; %s",
            Net_Label(app), msg.size,
            SDLNet_GetAddressString(destination.address),
            (int)destination.port,
            send_res ? "success" : "fail");
    }
}

static void Net_RecalculatePacketHeader(AppState *app)
{
    S8 payload = S8_Make(app->net.packet_payload_buf, app->net.payload_used);
    app->net.packet_header.magic_value = NET_MAGIC_VALUE;
    app->net.packet_header.payload_hash = (Uint16)S8_Hash(0, payload);
}

static S8 Net_GetPacketString(AppState *app)
{
    // there should be no padding between these
    static_assert(offsetof(AppState, net.packet_header) + sizeof(app->net.packet_header) ==
                  offsetof(AppState, net.packet_payload_buf));

    Uint64 total_size = sizeof(app->net.packet_header) + app->net.payload_used;
    S8 result = S8_Make((Uint8 *)&app->net.packet_header, total_size);
    return result;
}

static void Net_PacketSendAndResetPayload(AppState *app)
{
    Net_RecalculatePacketHeader(app);
    S8 packet = Net_GetPacketString(app);

    if (app->net.is_server)
    {
        ForU32(i, app->server.user_count)
        {
            Net_SendS8(app, app->server.users[i], packet);
        }
    }
    else
    {
        Net_SendS8(app, app->net.server_user, packet);
    }

    app->net.payload_used = 0;
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
    ForU32(i, app->server.user_count)
    {
        if (Net_UserMatchAddrPort(app->server.users[i], address, port))
            return app->server.users + i;
    }
    return 0;
}

static Net_User *Net_AddUser(AppState *app, SDLNet_Address *address, Uint16 port)
{
    if (app->server.user_count < ArrayCount(app->server.users))
    {
        Net_User *user = app->server.users + app->server.user_count;
        app->server.user_count += 1;
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

    if (is_server && !app->server.user_count)
    {
        return;
    }

    {
        // hacky temporary network activity rate-limitting
        static Uint64 last_timestamp = 0;
        if (app->frame_time < last_timestamp + 16)
            return;
        last_timestamp = app->frame_time;
    }

    // send network test
    if (0)
    {
        if (is_client)
        {
            Net_Cmd cmd = {};
            cmd.tick_id = app->tick_id;
            cmd.kind = NetCmd_NetworkTest;
            Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

            Net_Payload_NetworkTest test;
            ForArray(i, test.numbers)
                test.numbers[i] = i + 1;

            Net_PayloadMemcpy(app, &test, sizeof(test));
            Net_PacketSendAndResetPayload(app);
        }
    }

    if (is_server)
    {
        ForU32(net_index, OBJ_MAX_NETWORK_OBJECTS)
        {
            Object *net_obj = Object_FromNetIndex(app, net_index);

            if (Object_HasData(net_obj))
            {
                Net_Cmd cmd = {};
                cmd.tick_id = app->tick_id;
                cmd.kind = NetCmd_ObjUpdate;
                Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

                Net_ObjUpdate update = {0};
                update.net_index = net_index;
                update.obj = *net_obj;
                Net_PayloadMemcpy(app, &update, sizeof(update));
            }
            else
            {
                Net_Cmd cmd = {};
                cmd.tick_id = app->tick_id;
                cmd.kind = NetCmd_ObjEmpty;
                Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

                Net_ObjEmpty update = {0};
                update.net_index = net_index;
                Net_PayloadMemcpy(app, &update, sizeof(update));
            }

            Net_PacketSendAndResetPayload(app);
        }
    }

    if (is_client)
    {
        {
            Net_Cmd cmd = {};
            cmd.tick_id = app->tick_id;
            cmd.kind = NetCmd_Ping;
            Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

            Net_Ping ping = {0};
            Net_PayloadMemcpy(app, &ping, sizeof(ping));
            Net_PacketSendAndResetPayload(app);
        }


        {
            Net_Cmd cmd = {};
            cmd.tick_id = app->tick_id;
            cmd.kind = NetCmd_Inputs;
            Net_PayloadMemcpy(app, &cmd, sizeof(cmd));

            Net_Inputs inputs = {0};
            Uint16 input_count = 0;

            for (Uint64 i = app->client.tick_input_min;
                 i < app->client.tick_input_max;
                 i += 1)
            {
                if (input_count >= ArrayCount(inputs.inputs))
                {
                    Assert(false);
                    break;
                }

                Tick_Input *input = Client_InputAtTick(app, i);
                inputs.inputs[input_count] = *input;
                input_count += 1;
            }

            inputs.input_count = input_count;

            Net_PayloadMemcpy(app, &inputs, sizeof(inputs));
            Net_PacketSendAndResetPayload(app);
        }
    }
}

static void Net_ProcessReceivedPayload(AppState *app, Uint16 player_id, S8 full_message)
{
    S8 msg = full_message;
    while (msg.size)
    {
        Net_Cmd cmd;
        Net_ConsumeS8(&msg, &cmd, sizeof(cmd));

        if (cmd.kind == NetCmd_Ping)
        {
            Net_Ping ping = {0};
            Net_ConsumeS8(&msg, &ping, sizeof(ping));
        }
        else if (cmd.kind == NetCmd_ObjUpdate ||
                 cmd.kind == NetCmd_ObjEmpty)
        {
            Net_ObjUpdate update = {0};
            if (cmd.kind == NetCmd_ObjUpdate)
            {
                Net_ConsumeS8(&msg, &update, sizeof(update));
            }
            else
            {
                Net_ObjEmpty empty;
                Net_ConsumeS8(&msg, &empty, sizeof(empty));
                update.net_index = empty.net_index;
                update.obj.init = true;
            }

            if (update.net_index >= OBJ_MAX_NETWORK_OBJECTS)
            {
                LOG(LogFlags_NetPayload,
                    "%s: Rejecting payload(%d) - net index overflow: %u",
                    Net_Label(app), cmd.kind, update.net_index);
                continue;
            }

            if (app->client.next_playback_tick > cmd.tick_id)
            {
                LOG(LogFlags_NetPayload,
                    "%s: Rejecting payload(%d) - cmd tick at: %llu < next playback tick: %llu",
                    Net_Label(app), cmd.kind, cmd.tick_id, app->client.next_playback_tick);
                continue;
            }

            Assert(update.net_index < ArrayCount(app->client.obj_snaps));
            Client_Snapshot *snap = app->client.obj_snaps + update.net_index;
            Client_InsertSnapshotObject(app, snap, cmd.tick_id, update.obj);
        }
        else if (cmd.kind == NetCmd_NetworkTest)
        {
            Net_Payload_NetworkTest test;
            Net_ConsumeS8(&msg, &test, sizeof(test));

            ForArray(i, test.numbers)
            {
                Uint32 loaded = test.numbers[i];
                Uint32 expected = i + 1;
                bool compare = loaded == expected;
                Assert(compare);
            }
        }
        else if (cmd.kind == NetCmd_Inputs)
        {
            Net_Inputs in_net;
            Net_ConsumeS8(&msg, &in_net, sizeof(in_net));

            Assert(player_id < ArrayCount(app->server.player_inputs));
            Server_PlayerInputBuffer *in_buf = app->server.player_inputs + player_id;
            Server_PlayerInputBufferInsert(in_buf, &in_net, cmd.tick_id);
        }
        else
        {
            LOG(LogFlags_NetPayload,
                "%s: Unsupported payload cmd kind: %d",
                Net_Label(app), (int)cmd.kind);
            return;
        }
    }
}

static void Net_ReceivePacket(AppState *app, Uint16 player_id, S8 packet)
{
    if (packet.size < sizeof(Net_PacketHeader))
    {
        LOG(LogFlags_NetPacket,
            "%s: packet rejected - it's too small, size: %llu",
            Net_Label(app), packet.size);
        return;
    }

    Net_PacketHeader header;
    Net_ConsumeS8(&packet, &header, sizeof(header));

    if (!packet.size)
    {
        LOG(LogFlags_NetPacket,
            "%s: packet rejected - empty payload",
            Net_Label(app));
        return;
    }

    if (header.magic_value != NET_MAGIC_VALUE)
    {
        LOG(LogFlags_NetPacket,
            "%s: packet rejected - invalid magic value: %u; expected: %u",
            Net_Label(app), (Uint32)header.magic_value, NET_MAGIC_VALUE);
        return;
    }

    // validate hash
    Uint64 hash64 = S8_Hash(0, packet);
    Uint16 hash16 = (Uint16)hash64;
    if (hash16 != header.payload_hash)
    {
        LOG(LogFlags_NetPacket,
            "%s: packet rejected - invalid hash: %u; calculated: %u",
            Net_Label(app), header.payload_hash, hash16);
        return;
    }

    Net_ProcessReceivedPayload(app, player_id, packet);
}

static void Net_IterateReceive(AppState *app)
{
    bool is_server = app->net.is_server;
    bool is_client = !app->net.is_server;
    if (app->net.err) return;

    {
        // hacky temporary network activity rate-limitting
        static Uint64 last_timestamp = 0;
        if (app->frame_time < last_timestamp + 8)
            return;
        last_timestamp = app->frame_time;
    }

    for (;;)
    {
        SDLNet_Datagram *dgram = 0;
        int receive = SDLNet_ReceiveDatagram(app->net.socket, &dgram);
        if (!receive) break;
        if (!dgram) break;

        LOG(LogFlags_NetDatagram,
            "%s: got %d-byte datagram from %s:%d",
            Net_Label(app),
            (int)dgram->buflen,
            SDLNet_GetAddressString(dgram->addr),
            (int)dgram->port);

        if (is_client)
        {
            if (!Net_UserMatchAddrPort(app->net.server_user, dgram->addr, dgram->port))
            {
                LOG(LogFlags_NetDatagram,
                    "%s: dgram rejected - received from non-server address %s:%d",
                    Net_Label(app),
                    SDLNet_GetAddressString(dgram->addr), (int)dgram->port);
                goto datagram_cleanup;
            }
        }

        Uint16 player_id = 0;

        // save user
        if (is_server)
        {
            Net_User *user = Net_FindUser(app, dgram->addr, dgram->port);
            if (!user)
            {
                LOG(LogFlags_NetInfo,
                    "%s: saving user with port: %d",
                    Net_Label(app), (int)dgram->port);
                user = Net_AddUser(app, dgram->addr, dgram->port);
            }
        }

        S8 packet = S8_Make(dgram->buf, dgram->buflen);
        Net_ReceivePacket(app, player_id, packet);

        datagram_cleanup:
        SDLNet_DestroyDatagram(dgram);
    }
}

static void Net_Init(AppState *app)
{
    bool is_server = app->net.is_server;
    bool is_client = !app->net.is_server;

    LOG(LogFlags_NetInfo,
        is_server ? "Launching as server" : "Launching as client");

    if (is_client)
    {
        const char *hostname = "localhost";
        LOG(LogFlags_NetInfo,
            "%s: Resolving server hostname '%s' ...",
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
            LOG(LogFlags_NetInfo,
                "%s: Failed to resolve server hostname '%s'",
                Net_Label(app), hostname);
        }
    }

    Uint16 port = (app->net.is_server ? NET_DEFAULT_SEVER_PORT : 0);
    app->net.socket = SDLNet_CreateDatagramSocket(0, port);
    if (!app->net.socket)
    {
        app->net.err = true;
        LOG(LogFlags_NetInfo,
            "%s: Failed to create socket",
            Net_Label(app));
    }
    else
    {
        LOG(LogFlags_NetInfo,
            "%s: Created socket",
            Net_Label(app));

#if NET_SIMULATE_PACKETLOSS
        SDLNet_SimulateDatagramPacketLoss(app->net.socket, NET_SIMULATE_PACKETLOSS);
        LOG(LogFlags_NetInfo,
            "%s: Simulating packetloss: %d",
            Net_Label(app), NET_SIMULATE_PACKETLOSS);
#endif
    }
}

static void Net_Deinit()
{
    Assert(!"@todo");
}
