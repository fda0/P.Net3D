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

NET_User :: struct
{
  // address: *SDLNet_Address;
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
  inputs: [NET_MAX_INPUT_TICKS] TICK_Input;
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
  // @todo
}
