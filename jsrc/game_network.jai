NET_DEFAULT_SEVER_PORT :: 21037;
NET_MAGIC_VALUE :: 0xfda0;
NET_CLIENT_MAX_SNAPSHOTS :: TICK_RATE;
NET_MAX_INPUT_TICKS :: TICK_RATE/8;
NET_MAX_PLAYERS :: 10;
NET_MAX_PACKET_SIZE :: 1200;
NET_MAX_PAYLOAD_SIZE :: NET_MAX_PACKET_SIZE - size_of(NET_PacketHeader);
NET_SIMULATE_PACKETLOSS :: 0; // doesn't seem to work on localhost
NET_INACTIVE_MS :: 100;
NET_TIMEOUT_DISCONNECT_MS :: 250;

NET_State :: struct
{
  err: bool; // tracks if network is in error state
  is_server: bool;
  socket: *SDLNet_DatagramSocket;

  hacky_last_receive_timestamp: u64;

  server_user: NET_User;

  // msg payload
  packet_err: bool; // set on internal buffer overflow errors etc
  packet_header: NET_PacketHeader;
  packet_payload_buf: [1024 * 1024 * 1] u8; // 1 MB scratch buffer for network payload construction
  payload_used: u32;
};


// @impl
LOG_NetInfo :: 0;
LOG_NetDatagram :: 1;
LOG_NetPacket :: 2;
LOG_NetPayload :: 3;
LOG_NetClient :: 4;
LOG_NetCatchup :: 5;
LOG_NetCatchup :: 6;
LOG_NetTick :: 6;

Nlog :: (net_category: s64 /* @todo use user flags instead in the future */, format_string: string, args: .. Any, loc := #caller_location, flags := Log_Flags.NONE, user_flags : u32 = 0, section : *Log_Section = null)
{
  // @todo do something with net_category and logging in general
  // @todo add "NET_Label()" before every net log?, or do that for logs globally
  log(format_string, ..args, loc, flags, user_flags, section);
} @PrintLike

NET_User :: struct
{
  address: *SDLNet_Address;
  port: u16;
  last_msg_timestamp: u64;
};

NET_SendKind :: enum u32
{
  None;
  Ping :: 10000;
  ObjUpdate;
  ObjEmpty;
  NetworkTest;
  Inputs;
  AssignPlayerKey;
  WindowLayout;
};

NET_SendHeader :: struct
{
  tick_id: u64; // this is already send via packet header
  kind: NET_SendKind;
};

NET_SendInputs :: struct
{
  inputs: [NET_MAX_INPUT_TICKS] TICK_PlayerInput;
  input_count: u16;
};

NET_SendPing :: struct
{
  number: u64;
};

NET_SendObjSync :: struct
{
  net_index: u32;
  sync: OBJ_Sync;
};

NET_SendObjEmpty :: struct
{
  net_index: u32;
};

NET_SendAssignPlayerKey :: struct
{
  player_key: OBJ_Key;
};

NET_SendWindowLayout :: struct
{
  user_count: u32;
  px, py, w, h: s32;
};

NET_PacketHeader :: struct
{
  magic_value: u16; // use this as seed for hash calculation instead
  payload_hash: u16;
};

NET_Init :: ()
{
  is_server := G.net.is_server;
  is_client := !G.net.is_server;

  Nlog(LOG_NetInfo, ifx is_server then "Launching as server" else "Launching as client");

  if is_client
  {
    hostname := "localhost";
    Nlog(LOG_NetInfo, "Resolving server hostname '%s' ...", hostname);
    G.net.server_user.address = SDLNet_ResolveHostname(temp_c_string(hostname));
    G.net.server_user.port = NET_DEFAULT_SEVER_PORT;
    if G.net.server_user.address
    {
      if SDLNet_WaitUntilResolved(G.net.server_user.address, -1) < 0
      {
        SDLNet_UnrefAddress(G.net.server_user.address);
        G.net.server_user.address = null;
      }
    }

    if !G.net.server_user.address
    {
      G.net.err = true;
      Nlog(LOG_NetInfo, "Failed to resolve server hostname '%s'", hostname);
    }
  }

  port: u16 = xx ifx is_server then NET_DEFAULT_SEVER_PORT else 0;
  G.net.socket = SDLNet_CreateDatagramSocket(null, port);
  if !G.net.socket
  {
    G.net.err = true;
    Nlog(LOG_NetInfo, "Failed to create socket");
  }
  else
  {
    Nlog(LOG_NetInfo, "Created socket");

    #if NET_SIMULATE_PACKETLOSS
    {
      SDLNet_SimulateDatagramPacketLoss(G.net.socket, NET_SIMULATE_PACKETLOSS);
      Nlog(LOG_NetInfo, "%s: Simulating packetloss: %d", NET_SIMULATE_PACKETLOSS);
    }
  }
}

NET_IterateReceive :: ()
{
  is_server := G.net.is_server;
  is_client := !G.net.is_server;
  if G.net.err return;

  {
    // hacky temporary network activity rate-limitting
    if (G.timestamp < G.net.hacky_last_receive_timestamp + 8) return;
    G.net.hacky_last_receive_timestamp = G.timestamp;
  }

  while true
  {
    dgram: *SDLNet_Datagram;
    receive := SDLNet_ReceiveDatagram(G.net.socket, *dgram);
    if !receive break;
    if !dgram   break;

    Nlog(LOG_NetDatagram, "got %-byte datagram from %:%",
      dgram.buflen, SDLNet_GetAddressString(dgram.addr), dgram.port);

    dgram_error := false;
    if is_client
    {
      if !NET_UserMatchAddrPort(*G.net.server_user, dgram.addr, dgram.port)
      {
        dgram_error = true;
        Nlog(LOG_NetDatagram, "dgram rejected - received from non-server address %:%",
          SDLNet_GetAddressString(dgram.addr), dgram.port);
      }
    }

    if !dgram_error
    {
      player_id: u16 = NET_MAX_PLAYERS;

      // save user
      if is_server
      {
        user := NET_FindUser(dgram.addr, dgram.port);
        if (!user)
        {
          Nlog(LOG_NetInfo, "saving user with port: %", dgram.port);
          user = NET_AddUser(dgram.addr, dgram.port);
        }

        if (user)
        {
          user.last_msg_timestamp = G.timestamp;

          user_id: u64 = (user.(u64) - (*G.server.users[0]).(u64)) / size_of(NET_User);
          player_id = user_id.(u16);
        }
      }

      packet := string.{dgram.buflen, dgram.buf};
      NET_ReceivePacket(player_id, packet);
    }

    SDLNet_DestroyDatagram(dgram);
  }
}

NET_IterateSend :: ()
{
  // @impl
}

NET_IterateTimeoutUsers :: ()
{
  // @impl
}


NET_UserMatch :: (a: *NET_User, b: *NET_User) -> bool
{
  return (a.port == b.port && SDLNet_CompareAddresses(a.address, b.address) == 0);
}

NET_UserMatchAddrPort :: (user: *NET_User, address: *SDLNet_Address, port: u16) -> bool
{
  if (!user.address)
    return false;

  return (user.port == port && SDLNet_CompareAddresses(user.address, address) == 0);
}

NET_FindUser :: (address: *SDLNet_Address, port: u16) -> *NET_User
{
  // @todo move to SV_ prefix?
  for * G.server.users
  {
    if (NET_UserMatchAddrPort(it, address, port))
      return user;
  }
  return null;
}

NET_AddUser :: (address: *SDLNet_Address, port: u16) -> *NET_User
{
  // @todo move to SV_ prefix?
  new_user_slot: *NET_User;
  for * G.server.users
  {
    if (!it.address)
    {
      new_user_slot = it;
      break;
    }
  }

  if (new_user_slot)
  {
    new_user_slot.address = SDLNet_RefAddress(address);
    new_user_slot.port = port;
  }
  return new_user_slot;
}

NET_Consume:: ($T: Type, packet: *string) -> T
{
  value: T;
  to_copy := STR_Prefix(packet.*, size_of(T));
  memcpy(*value, to_copy.data, to_copy.count);
  packet.* = STR_Skip(packet.*, to_copy.count);
  return value;
}

NET_ReceivePacket :: (player_id: u16, original_packet: string)
{
  packet := original_packet;

  if G.net.is_server && player_id >= NET_MAX_PLAYERS
    return;

  if packet.count < size_of(NET_PacketHeader)
  {
    Nlog(LOG_NetPacket, "packet rejected - it's too small, size: %llu", packet.count);
    return;
  }

  header := NET_Consume(NET_PacketHeader, *packet);

  if (!packet.count)
  {
    Nlog(LOG_NetPacket, "packet rejected - empty payload",);
    return;
  }

  if (header.magic_value != NET_MAGIC_VALUE)
  {
    Nlog(LOG_NetPacket, "packet rejected - invalid magic value: %; expected: %", header.magic_value, NET_MAGIC_VALUE);
    return;
  }

  // validate hash
  hash64 := Hash64Any(packet);
  hash16 := cast,no_check(u16) hash64;
  if (hash16 != header.payload_hash)
  {
    Nlog(LOG_NetPacket, "packet rejected - invalid hash: %; calculated: %", header.payload_hash, hash16);
    return;
  }

  NET_ProcessReceivedPayload(player_id, packet);
}

NET_ProcessReceivedPayload :: (player_id: u16, full_message: string)
{
  msg := full_message;
  while msg.count
  {
    head := NET_Consume(NET_SendHeader, *msg);

    if head.kind == .Ping
    {
      ping := NET_Consume(NET_SendPing, *msg);
    }
    else if (head.kind == .ObjUpdate ||
             head.kind == .ObjEmpty)
    {
      update: NET_SendObjSync;
      if (head.kind == .ObjUpdate)
      {
        update = NET_Consume(NET_SendObjSync, *msg);
      }
      else
      {
        empty := NET_Consume(NET_SendObjEmpty, *msg);
        update.net_index = empty.net_index;
        update.sync.init = true;
      }

      if update.net_index >= OBJ_MAX_NETWORK_OBJECTS
      {
        Nlog(LOG_NetPayload, "Rejecting payload(%) - net index overflow: %", head.kind, update.net_index);
        continue;
      }

      if G.client.next_playback_tick > head.tick_id
      {
        Nlog(LOG_NetPayload, "Rejecting payload(%) - head tick at: % < next playback tick: %",
            head.kind, head.tick_id, G.client.next_playback_tick);
        continue;
      }

      snap := *G.client.snaps_of_objs[update.net_index];
      CLIENT_InsertSnapshot(snap, head.tick_id, update.sync);
    }
    else if head.kind == .Inputs
    {
      in_net := NET_Consume(NET_SendInputs, *msg);
      pi := *G.server.player_inputs[player_id];
      SERVER_InsertPlayerInput(pi, *in_net, head.tick_id);
    }
    else if head.kind == .AssignPlayerKey
    {
      assign := NET_Consume(NET_SendAssignPlayerKey, *msg);

      if G.client.player_key_latest_tick_id < head.tick_id
      {
        G.client.player_key = assign.player_key;
        G.client.player_key_latest_tick_id = head.tick_id;
      }
    }
    else if head.kind == .WindowLayout
    {
      layout := NET_Consume(NET_SendWindowLayout, *msg);
      if G.window_autolayout
        GAME_AutoLayoutApply(layout.user_count, layout.px, layout.py, layout.w, layout.h);
    }
    else
    {
      Nlog(LOG_NetPayload, "Unsupported payload head kind: %d", head.kind);
      return;
    }
  }
}
