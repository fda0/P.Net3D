static bool NET_IsServer()
{
  return APP.net.is_server;
}

static bool NET_IsClient()
{
  return !APP.net.is_server;
}

static const char *NET_Label()
{
  return APP.net.is_server ? "SERVER" : "CLIENT";
}

static bool NET_UserIsInactive(NET_User *user)
{
  return (user->last_msg_timestamp + NET_INACTIVE_MS < APP.timestamp);
}

static U8 *NET_PayloadAlloc(U32 size)
{
  U8 *result = APP.net.packet_payload_buf + APP.net.payload_used;
  {
    // return ptr to start of the buffer
    // on overflow
    U8 *end = (APP.net.packet_payload_buf + sizeof(APP.net.packet_payload_buf));
    if (end < (result + size))
    {
      Assert(false);
      result = APP.net.packet_payload_buf;
      APP.net.packet_err = true;
    }
  }
  APP.net.payload_used += size;
  return result;
}

static U8 *NET_PayloadMemcpy(void *data, U32 size)
{
  U8 *result = NET_PayloadAlloc(size);
  memcpy(result, data, size);
  return result;
}

static void NET_SendS8(NET_User *destination, S8 msg)
{
  Assert(msg.size < 1280); // that seems to be a realistic reasonable max size? https://gafferongames.com/post/packet_fragmentation_and_reassembly/

  if (APP.net.is_server && NET_UserIsInactive(destination))
    return;

  bool send_res = SDLNet_SendDatagram(APP.net.socket,
                                      destination->address,
                                      destination->port,
                                      msg.str, (U32)msg.size);

  if (!send_res)
  {
    LOG(LogFlags_NetSend,
        "%s: Sending buffer of size %lluB to %s:%d; %s",
        NET_Label(), msg.size,
        SDLNet_GetAddressString(destination->address),
        (int)destination->port,
        send_res ? "success" : "fail");
  }
}

static void NET_RecalculatePacketHeader()
{
  S8 payload = S8_Make(APP.net.packet_payload_buf, APP.net.payload_used);
  APP.net.packet_header.magic_value = NET_MAGIC_VALUE;
  APP.net.packet_header.payload_hash = (U16)S8_Hash(0, payload);
}

static S8 NET_GetPacketString()
{
  // there should be no padding between these
  static_assert(offsetof(AppState, net.packet_header) + sizeof(APP.net.packet_header) ==
                offsetof(AppState, net.packet_payload_buf));

  U64 total_size = sizeof(APP.net.packet_header) + APP.net.payload_used;
  S8 result = S8_Make((U8 *)&APP.net.packet_header, total_size);
  return result;
}

static void NET_PacketSendAndResetPayload(NET_User *destination /* null for broadcast */)
{
  NET_RecalculatePacketHeader();
  S8 packet = NET_GetPacketString();

  if (destination)
  {
    NET_SendS8(destination, packet);
  }
  else
  {
    if (APP.net.is_server)
    {
      ForArray(user_index, APP.server.users)
      {
        NET_User *user = APP.server.users + user_index;
        if (!user->address)
          continue;

        NET_SendS8(user, packet);
      }
    }
    else
    {
      NET_SendS8(&APP.net.server_user, packet);
    }
  }

  APP.net.payload_used = 0;
}

static bool NET_ConsumeS8(S8 *msg, void *dest, U64 size)
{
  U64 copy_size = Min(size, msg->size);
  bool err = copy_size != size;

  if (err)
    memset(dest, 0, size); // fill dest with zeroes

  memcpy(dest, msg->str, copy_size);
  *msg = S8_Skip(*msg, size);

  return err;
}

static bool NET_UserMatch(NET_User *a, NET_User *b)
{
  return (a->port == b->port &&
          SDLNet_CompareAddresses(a->address, b->address) == 0);
}

static bool NET_UserMatchAddrPort(NET_User *user, SDLNet_Address *address, U16 port)
{
  if (!user->address)
    return false;

  return (user->port == port &&
          SDLNet_CompareAddresses(user->address, address) == 0);
}

static NET_User *NET_FindUser(SDLNet_Address *address, U16 port)
{
  ForArray(i, APP.server.users)
  {
    NET_User *user = APP.server.users + i;
    if (NET_UserMatchAddrPort(user, address, port))
      return user;
  }
  return 0;
}

static NET_User *NET_AddUser(SDLNet_Address *address, U16 port)
{
  NET_User *new_user_slot = 0;
  ForArray(i, APP.server.users)
  {
    NET_User *user = APP.server.users + i;
    if (!user->address)
    {
      new_user_slot = user;
      break;
    }
  }

  if (new_user_slot)
  {
    new_user_slot->address = SDLNet_RefAddress(address);
    new_user_slot->port = port;
  }
  return new_user_slot;
}

static void NET_RemoveUser(NET_User *user)
{
  SDLNet_UnrefAddress(user->address);
  *user = (NET_User){};
}

static void NET_IterateSend()
{
  bool is_server = APP.net.is_server;
  bool is_client = !APP.net.is_server;
  if (APP.net.err) return;

  {
    // hacky temporary network activity rate-limitting
    static U64 last_timestamp = 0;
    if (APP.timestamp < last_timestamp + 16)
      return;
    last_timestamp = APP.timestamp;
  }

  if (is_server)
  {
    U32 client_count = 0;
    ForArray(i, APP.server.users)
    {
      if (APP.server.users[i].address)
        client_count += 1;
    }

    // iterate over connected users
    U32 user_number = 0;
    ForArray(user_index, APP.server.users)
    {
      NET_User *user = APP.server.users + user_index;
      if (!user->address)
        continue;

      user_number += 1;

      // create player characters and send them to users
      {
        AssertBounds(user_index, APP.server.player_keys);
        OBJ_Key *player_key = APP.server.player_keys + user_index;

        if (!player_key->serial_number)
        {
          Object *player = OBJ_CreatePlayer();
          if (!OBJ_IsNil(player))
          {
            player->s.p.y = -150.f + user_index * 70.f;
            V3 color = {};
            color.x = (user_index & 4) ? .3f : 0.8f;
            color.y = (user_index & 2) ? .3f : 0.8f;
            color.z = (user_index & 1) ? .3f : 0.8f;
            player->s.color = Color32_V3(color);
          }
          *player_key = player->s.key;
        }

        NET_SendHeader head = {};
        head.tick_id = APP.tick_id;
        head.kind = NetSendKind_AssignPlayerKey;
        NET_PayloadMemcpy(&head, sizeof(head));

        NET_SendAssignPlayerKey assign = {0};
        assign.player_key = *player_key;
        NET_PayloadMemcpy(&assign, sizeof(assign));

        NET_PacketSendAndResetPayload(user);
      }

      // calculate autolayout for clients
      // @todo this doesn't have to be done on iterate send
      if (APP.window_autolayout)
      {
        U32 window_count = 1 + client_count;

        U32 rows = 1;
        U32 cols = 1;
        ForU32(timeout, 16)
        {
          U32 total_slots = rows * cols;
          if (total_slots >= window_count)
            break;

          if (rows > cols) cols += 1;
          else rows += 1;
        }

        U32 win_x = APP.init_window_px;
        U32 win_y = APP.init_window_py;
        U32 win_w = APP.init_window_width / cols;
        U32 win_h = APP.init_window_height / rows;

        // calc server window
        Game_AutoLayoutApply(client_count, win_x, win_y, win_w, win_h);

        // calc client window
        {
          U32 window_index = user_number;
          U32 x = window_index / rows;
          U32 y = window_index % rows;

          NET_SendHeader head = {};
          head.tick_id = APP.tick_id;
          head.kind = NetSendKind_WindowLayout;
          NET_PayloadMemcpy(&head, sizeof(head));

          NET_SendWindowLayout body = {};
          body.user_count = client_count;
          body.px = win_x + x*win_w;
          body.py = win_y + y*win_h;
          body.w = win_w;
          body.h = win_h;
          NET_PayloadMemcpy(&body, sizeof(body));

          NET_PacketSendAndResetPayload(user);
        }
      }
    }

    // iterate over network objects
    ForU32(net_index, OBJ_MAX_NETWORK_OBJECTS)
    {
      Object *net_obj = OBJ_FromNetIndex(net_index);

      if (OBJ_HasData(net_obj))
      {
        NET_SendHeader head = {};
        head.tick_id = APP.tick_id;
        head.kind = NetSendKind_ObjUpdate;
        NET_PayloadMemcpy(&head, sizeof(head));

        NET_SendObjSync update = {0};
        update.net_index = net_index;
        update.sync = net_obj->s;
        NET_PayloadMemcpy(&update, sizeof(update));
      }
      else
      {
        NET_SendHeader head = {};
        head.tick_id = APP.tick_id;
        head.kind = NetSendKind_ObjEmpty;
        NET_PayloadMemcpy(&head, sizeof(head));

        NET_SendObjEmpty update = {0};
        update.net_index = net_index;
        NET_PayloadMemcpy(&update, sizeof(update));
      }

      NET_PacketSendAndResetPayload(0);
    }
  }

  if (is_client)
  {
    {
      NET_SendHeader head = {};
      head.tick_id = APP.tick_id;
      head.kind = NetSendKind_Ping;
      NET_PayloadMemcpy(&head, sizeof(head));

      NET_SendPing ping = {0};
      NET_PayloadMemcpy(&ping, sizeof(ping));
      NET_PacketSendAndResetPayload(0);
    }


    {
      CLIENT_PollInput();

      NET_SendHeader head = {};
      head.tick_id = APP.tick_id;
      head.kind = NetSendKind_Inputs;
      NET_PayloadMemcpy(&head, sizeof(head));

      NET_SendInputs inputs = {0};
      U16 input_count = 0;

      for (U64 i = APP.client.inputs_range.min;
           i < APP.client.inputs_range.max;
           i += 1)
      {
        if (input_count >= ArrayCount(inputs.inputs))
        {
          Assert(false);
          break;
        }

        TICK_Input *peek = Q_PeekAt(APP.client.inputs_qbuf,
                                    &APP.client.inputs_range,
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

      NET_PayloadMemcpy(&inputs, sizeof(inputs));
      NET_PacketSendAndResetPayload(0);
    }
  }
}

static void NET_ProcessReceivedPayload(U16 player_id, S8 full_message)
{
  S8 msg = full_message;
  while (msg.size)
  {
    NET_SendHeader head;
    NET_ConsumeS8(&msg, &head, sizeof(head));

    if (head.kind == NetSendKind_Ping)
    {
      NET_SendPing ping = {0};
      NET_ConsumeS8(&msg, &ping, sizeof(ping));
    }
    else if (head.kind == NetSendKind_ObjUpdate ||
             head.kind == NetSendKind_ObjEmpty)
    {
      NET_SendObjSync update = {0};
      if (head.kind == NetSendKind_ObjUpdate)
      {
        NET_ConsumeS8(&msg, &update, sizeof(update));
      }
      else
      {
        NET_SendObjEmpty empty;
        NET_ConsumeS8(&msg, &empty, sizeof(empty));
        update.net_index = empty.net_index;
        update.sync.init = true;
      }

      if (update.net_index >= OBJ_MAX_NETWORK_OBJECTS)
      {
        LOG(LogFlags_NetPayload,
            "%s: Rejecting payload(%d) - net index overflow: %u",
            NET_Label(), head.kind, update.net_index);
        continue;
      }

      if (APP.client.next_playback_tick > head.tick_id)
      {
        LOG(LogFlags_NetPayload,
            "%s: Rejecting payload(%d) - head tick at: %llu < next playback tick: %llu",
            NET_Label(), head.kind, head.tick_id, APP.client.next_playback_tick);
        continue;
      }

      Assert(update.net_index < ArrayCount(APP.client.snaps_of_objs));
      CLIENT_ObjSnapshots *snap = APP.client.snaps_of_objs + update.net_index;
      CLIENT_InsertSnapshot(snap, head.tick_id, update.sync);
    }
    else if (head.kind == NetSendKind_Inputs)
    {
      NET_SendInputs in_net;
      NET_ConsumeS8(&msg, &in_net, sizeof(in_net));

      Assert(player_id < ArrayCount(APP.server.player_inputs));
      SERVER_PlayerInputs *pi = APP.server.player_inputs + player_id;
      SERVER_InsertPlayerInput(pi, &in_net, head.tick_id);
    }
    else if (head.kind == NetSendKind_AssignPlayerKey)
    {
      NET_SendAssignPlayerKey assign;
      NET_ConsumeS8(&msg, &assign, sizeof(assign));

      if (APP.client.player_key_latest_tick_id < head.tick_id)
      {
        APP.client.player_key = assign.player_key;
        APP.client.player_key_latest_tick_id = head.tick_id;
      }
    }
    else if (head.kind == NetSendKind_WindowLayout)
    {
      NET_SendWindowLayout layout;
      NET_ConsumeS8(&msg, &layout, sizeof(layout));

      if (APP.window_autolayout)
      {
        Game_AutoLayoutApply(layout.user_count,
                             layout.px, layout.py, layout.w, layout.h);
      }
    }
    else
    {
      LOG(LogFlags_NetPayload,
          "%s: Unsupported payload head kind: %d",
          NET_Label(), (int)head.kind);
      return;
    }
  }
}

static void NET_ReceivePacket(U16 player_id, S8 packet)
{
  if (APP.net.is_server && player_id >= NET_MAX_PLAYERS)
    return;

  if (packet.size < sizeof(NET_PacketHeader))
  {
    LOG(LogFlags_NetPacket,
        "%s: packet rejected - it's too small, size: %llu",
        NET_Label(), packet.size);
    return;
  }

  NET_PacketHeader header;
  NET_ConsumeS8(&packet, &header, sizeof(header));

  if (!packet.size)
  {
    LOG(LogFlags_NetPacket,
        "%s: packet rejected - empty payload",
        NET_Label());
    return;
  }

  if (header.magic_value != NET_MAGIC_VALUE)
  {
    LOG(LogFlags_NetPacket,
        "%s: packet rejected - invalid magic value: %u; expected: %u",
        NET_Label(), (U32)header.magic_value, NET_MAGIC_VALUE);
    return;
  }

  // validate hash
  U64 hash64 = S8_Hash(0, packet);
  U16 hash16 = (U16)hash64;
  if (hash16 != header.payload_hash)
  {
    LOG(LogFlags_NetPacket,
        "%s: packet rejected - invalid hash: %u; calculated: %u",
        NET_Label(), header.payload_hash, hash16);
    return;
  }

  NET_ProcessReceivedPayload(player_id, packet);
}

static void NET_IterateReceive()
{
  bool is_server = APP.net.is_server;
  bool is_client = !APP.net.is_server;
  if (APP.net.err) return;

  {
    // hacky temporary network activity rate-limitting
    static U64 last_timestamp = 0;
    if (APP.timestamp < last_timestamp + 8)
      return;
    last_timestamp = APP.timestamp;
  }

  for (;;)
  {
    SDLNet_Datagram *dgram = 0;
    int receive = SDLNet_ReceiveDatagram(APP.net.socket, &dgram);
    if (!receive) break;
    if (!dgram) break;

    LOG(LogFlags_NetDatagram,
        "%s: got %d-byte datagram from %s:%d",
        NET_Label(),
        (int)dgram->buflen,
        SDLNet_GetAddressString(dgram->addr),
        (int)dgram->port);

    if (is_client)
    {
      if (!NET_UserMatchAddrPort(&APP.net.server_user, dgram->addr, dgram->port))
      {
        LOG(LogFlags_NetDatagram,
            "%s: dgram rejected - received from non-server address %s:%d",
            NET_Label(),
            SDLNet_GetAddressString(dgram->addr), (int)dgram->port);
        goto datagram_cleanup;
      }
    }


    U16 player_id = NET_MAX_PLAYERS;

    // save user
    if (is_server)
    {
      NET_User *user = NET_FindUser(dgram->addr, dgram->port);
      if (!user)
      {
        LOG(LogFlags_NetInfo,
            "%s: saving user with port: %d",
            NET_Label(), (int)dgram->port);
        user = NET_AddUser(dgram->addr, dgram->port);
      }

      if (user)
      {
        user->last_msg_timestamp = APP.timestamp;

        U64 user_id = ((U64)user - (U64)APP.server.users) / sizeof(NET_User);
        player_id = Checked_U64toU16(user_id);
      }
    }

    S8 packet = S8_Make(dgram->buf, dgram->buflen);
    NET_ReceivePacket(player_id, packet);

    datagram_cleanup:
    SDLNet_DestroyDatagram(dgram);
  }
}

static void NET_IterateTimeoutUsers()
{
  if (NET_IsServer())
  {
    ForArray(user_index, APP.server.users)
    {
      NET_User *user = APP.server.users + user_index;
      if (!user->address)
        continue;

      if (user->last_msg_timestamp + NET_TIMEOUT_DISCONNECT_MS < APP.timestamp)
      {
        LOG(LogFlags_NetInfo,
            "%s: Timeout. Removing user #%llu",
            NET_Label(), user_index);

        NET_RemoveUser(user);
      }
    }
  }
}

static void NET_Init()
{
  bool is_server = APP.net.is_server;
  bool is_client = !APP.net.is_server;

  LOG(LogFlags_NetInfo,
      is_server ? "Launching as server" : "Launching as client");

  if (is_client)
  {
    const char *hostname = "localhost";
    LOG(LogFlags_NetInfo,
        "%s: Resolving server hostname '%s' ...",
        NET_Label(), hostname);
    APP.net.server_user.address = SDLNet_ResolveHostname(hostname);
    APP.net.server_user.port = NET_DEFAULT_SEVER_PORT;
    if (APP.net.server_user.address)
    {
      if (SDLNet_WaitUntilResolved(APP.net.server_user.address, -1) < 0)
      {
        SDLNet_UnrefAddress(APP.net.server_user.address);
        APP.net.server_user.address = 0;
      }
    }

    if (!APP.net.server_user.address)
    {
      APP.net.err = true;
      LOG(LogFlags_NetInfo,
          "%s: Failed to resolve server hostname '%s'",
          NET_Label(), hostname);
    }
  }

  U16 port = (APP.net.is_server ? NET_DEFAULT_SEVER_PORT : 0);
  APP.net.socket = SDLNet_CreateDatagramSocket(0, port);
  if (!APP.net.socket)
  {
    APP.net.err = true;
    LOG(LogFlags_NetInfo,
        "%s: Failed to create socket",
        NET_Label());
  }
  else
  {
    LOG(LogFlags_NetInfo,
        "%s: Created socket",
        NET_Label());

#if NET_SIMULATE_PACKETLOSS
    SDLNet_SimulateDatagramPacketLoss(APP.net.socket, NET_SIMULATE_PACKETLOSS);
    LOG(LogFlags_NetInfo,
        "%s: Simulating packetloss: %d",
        NET_Label(), NET_SIMULATE_PACKETLOSS);
#endif
  }
}

static void NET_Deinit()
{
  Assert(!"@todo");
}
