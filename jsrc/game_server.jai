SERVER_PlayerInputs :: struct
{
  TICK_Input qbuf[NET_MAX_INPUT_TICKS];
  RngU64 playback_range;
  U64 latest_client_tick_id;
  TICK_Input last_input;
  TickDeltas receive_deltas;
};

SERVER_State :: struct
{
  NET_User users[NET_MAX_PLAYERS];
  OBJ_Key player_keys[NET_MAX_PLAYERS];
  SERVER_PlayerInputs player_inputs[NET_MAX_PLAYERS];
};
