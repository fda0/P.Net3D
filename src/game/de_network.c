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
static const char *Net_Label2()
{
    return APP.net.is_server ? "SERVER" : "CLIENT";
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

static void Net_PacketSendAndResetPayload(AppState *app,
                                          Net_User *destination /* null for broadcast */)
{
    Net_RecalculatePacketHeader(app);
    S8 packet = Net_GetPacketString(app);

    if (destination)
    {
        Net_SendS8(app, *destination, packet);
    }
    else
    {
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
            Net_SendHeader head = {};
            head.tick_id = app->tick_id;
            head.kind = NetSendKind_NetworkTest;
            Net_PayloadMemcpy(app, &head, sizeof(head));

            Net_SendNetworkTest test;
            ForArray(i, test.numbers)
                test.numbers[i] = i + 1;

            Net_PayloadMemcpy(app, &test, sizeof(test));
            Net_PacketSendAndResetPayload(app, 0);
        }
    }

    if (is_server)
    {
        // iterate over connected users
        ForU32(user_index, app->server.user_count)
        {
            Net_User *user = app->server.users + user_index;
            if (user->address)
            {
                // create player characters and send them to users
                {
                    AssertBounds(user_index, app->server.player_keys);
                    Object_Key *player_key = app->server.player_keys + user_index;

                    if (!player_key->made_at_tick)
                    {
                        Object *player = Object_CreatePlayer(app);
                        if (!Object_IsNil(player))
                        {
                            player->p.x = 0.f + user_index * 1.f;
                            player->sprite_color.g = (user_index & 1) ? 1.f : 0.f;
                            player->sprite_color.b = (user_index & 2) ? 1.f : 0.f;
                        }
                        *player_key = player->key;
                    }

                    Net_SendHeader head = {};
                    head.tick_id = app->tick_id;
                    head.kind = NetSendKind_AssignPlayerKey;
                    Net_PayloadMemcpy(app, &head, sizeof(head));

                    Net_SendAssignPlayerKey assign = {0};
                    assign.player_key = *player_key;
                    Net_PayloadMemcpy(app, &assign, sizeof(assign));

                    Net_PacketSendAndResetPayload(app, user);
                }

                // calculate autolayout for clients
                // @todo this should be done on iterate send!
                if (app->window_autolayout)
                {
                    Uint32 window_count = 1 + app->server.user_count;
                    Uint32 side_chunks = 1;
                    ForU32(timeout, 8)
                    {
                        Uint32 total_chunks = side_chunks * side_chunks;
                        if (total_chunks >= window_count)
                            break;
                        side_chunks += 1;
                    }

                    Uint32 win_x = app->init_window_px;
                    Uint32 win_y = app->init_window_py;
                    Uint32 win_w = app->init_window_width / side_chunks;
                    Uint32 win_h = app->init_window_height / side_chunks;

                    // calc server window
                    {
                        Uint32 window_index = 0;
                        Uint32 x = window_index / side_chunks;
                        Uint32 y = window_index % side_chunks;

                        Game_AutoLayoutApply(app, app->server.user_count,
                                             win_x + x*win_w, win_y + y*win_h,
                                             win_w, win_h);
                    }

                    // calc client window
                    {
                        Uint32 window_index = user_index + 1;
                        Uint32 x = window_index / side_chunks;
                        Uint32 y = window_index % side_chunks;

                        Net_SendHeader head = {};
                        head.tick_id = app->tick_id;
                        head.kind = NetSendKind_WindowLayout;
                        Net_PayloadMemcpy(app, &head, sizeof(head));

                        Net_SendWindowLayout body = {0};
                        body.user_count = app->server.user_count;
                        body.px = win_x + x*win_w;
                        body.py = win_y + y*win_h;
                        body.w = win_w;
                        body.h = win_h;
                        Net_PayloadMemcpy(app, &body, sizeof(body));

                        Net_PacketSendAndResetPayload(app, user);
                    }
                }
            }
        }

        // iterate over network objects
        ForU32(net_index, OBJ_MAX_NETWORK_OBJECTS)
        {
            Object *net_obj = Object_FromNetIndex(app, net_index);

            if (Object_HasData(net_obj))
            {
                Net_SendHeader head = {};
                head.tick_id = app->tick_id;
                head.kind = NetSendKind_ObjUpdate;
                Net_PayloadMemcpy(app, &head, sizeof(head));

                Net_SendObjUpdate update = {0};
                update.net_index = net_index;
                update.obj = *net_obj;
                Net_PayloadMemcpy(app, &update, sizeof(update));
            }
            else
            {
                Net_SendHeader head = {};
                head.tick_id = app->tick_id;
                head.kind = NetSendKind_ObjEmpty;
                Net_PayloadMemcpy(app, &head, sizeof(head));

                Net_SendObjEmpty update = {0};
                update.net_index = net_index;
                Net_PayloadMemcpy(app, &update, sizeof(update));
            }

            Net_PacketSendAndResetPayload(app, 0);
        }
    }

    if (is_client)
    {
        {
            Net_SendHeader head = {};
            head.tick_id = app->tick_id;
            head.kind = NetSendKind_Ping;
            Net_PayloadMemcpy(app, &head, sizeof(head));

            Net_SendPing ping = {0};
            Net_PayloadMemcpy(app, &ping, sizeof(ping));
            Net_PacketSendAndResetPayload(app, 0);
        }


        {
            Client_PollInput(app);

            Net_SendHeader head = {};
            head.tick_id = app->tick_id;
            head.kind = NetSendKind_Inputs;
            Net_PayloadMemcpy(app, &head, sizeof(head));

            Net_SendInputs inputs = {0};
            Uint16 input_count = 0;

            for (Uint64 i = app->client.inputs_range.min;
                 i < app->client.inputs_range.max;
                 i += 1)
            {
                if (input_count >= ArrayCount(inputs.inputs))
                {
                    Assert(false);
                    break;
                }

                Tick_Input *peek = Q_PeekAt(app->client.inputs_qbuf,
                                            &app->client.inputs_range,
                                            i);
                if (!peek)
                {
                    Assert(false);
                    break;
                }

                inputs.inputs[input_count] = *peek;
                input_count += 1;
            }

            inputs.input_count = input_count;

            Net_PayloadMemcpy(app, &inputs, sizeof(inputs));
            Net_PacketSendAndResetPayload(app, 0);
        }
    }
}

static void Net_ProcessReceivedPayload(AppState *app, Uint16 player_id, S8 full_message)
{
    S8 msg = full_message;
    while (msg.size)
    {
        Net_SendHeader head;
        Net_ConsumeS8(&msg, &head, sizeof(head));

        if (head.kind == NetSendKind_Ping)
        {
            Net_SendPing ping = {0};
            Net_ConsumeS8(&msg, &ping, sizeof(ping));
        }
        else if (head.kind == NetSendKind_ObjUpdate ||
                 head.kind == NetSendKind_ObjEmpty)
        {
            Net_SendObjUpdate update = {0};
            if (head.kind == NetSendKind_ObjUpdate)
            {
                Net_ConsumeS8(&msg, &update, sizeof(update));
            }
            else
            {
                Net_SendObjEmpty empty;
                Net_ConsumeS8(&msg, &empty, sizeof(empty));
                update.net_index = empty.net_index;
                update.obj.init = true;
            }

            if (update.net_index >= OBJ_MAX_NETWORK_OBJECTS)
            {
                LOG(LogFlags_NetPayload,
                    "%s: Rejecting payload(%d) - net index overflow: %u",
                    Net_Label(app), head.kind, update.net_index);
                continue;
            }

            if (app->client.next_playback_tick > head.tick_id)
            {
                LOG(LogFlags_NetPayload,
                    "%s: Rejecting payload(%d) - head tick at: %llu < next playback tick: %llu",
                    Net_Label(app), head.kind, head.tick_id, app->client.next_playback_tick);
                continue;
            }

            Assert(update.net_index < ArrayCount(app->client.obj_snaps));
            Client_Snapshot *snap = app->client.obj_snaps + update.net_index;
            Client_InsertSnapshotObject(app, snap, head.tick_id, update.obj);
        }
        else if (head.kind == NetSendKind_NetworkTest)
        {
            Net_SendNetworkTest test;
            Net_ConsumeS8(&msg, &test, sizeof(test));

            ForArray(i, test.numbers)
            {
                Uint32 loaded = test.numbers[i];
                Uint32 expected = i + 1;
                bool compare = loaded == expected;
                Assert(compare);
            }
        }
        else if (head.kind == NetSendKind_Inputs)
        {
            Net_SendInputs in_net;
            Net_ConsumeS8(&msg, &in_net, sizeof(in_net));

            Assert(player_id < ArrayCount(app->server.player_inputs));
            Server_PlayerInputs *pi = app->server.player_inputs + player_id;
            Server_InsertPlayerInput(pi, &in_net, head.tick_id);
        }
        else if (head.kind == NetSendKind_AssignPlayerKey)
        {
            Net_SendAssignPlayerKey assign;
            Net_ConsumeS8(&msg, &assign, sizeof(assign));

            if (app->client.player_key_latest_tick_id < head.tick_id)
            {
                app->client.player_key = assign.player_key;
                app->client.player_key_latest_tick_id = head.tick_id;
            }
        }
        else if (head.kind == NetSendKind_WindowLayout)
        {
            Net_SendWindowLayout layout;
            Net_ConsumeS8(&msg, &layout, sizeof(layout));

            Game_AutoLayoutApply(app, layout.user_count,
                                 layout.px, layout.py, layout.w, layout.h);
        }
        else
        {
            LOG(LogFlags_NetPayload,
                "%s: Unsupported payload head kind: %d",
                Net_Label(app), (int)head.kind);
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

            if (user)
            {
                Uint64 user_id = ((Uint64)user - (Uint64)app->server.users) / sizeof(Net_User);
                if (user_id < NET_MAX_PLAYERS)
                {
                    player_id = user_id;
                }
                else
                {
                    Assert(0);
                }
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
