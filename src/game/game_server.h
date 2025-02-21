typedef struct
{
    Tick_Input qbuf[NET_MAX_INPUT_TICKS];
    RngU64 playback_range;
    U64 latest_client_tick_id;
    Tick_Input last_input;
    TickDeltas receive_deltas;
} Server_PlayerInputs;

typedef struct
{
    Net_User users[NET_MAX_PLAYERS];
    Obj_Key player_keys[NET_MAX_PLAYERS];
    Server_PlayerInputs player_inputs[NET_MAX_PLAYERS];
} Server_State;
