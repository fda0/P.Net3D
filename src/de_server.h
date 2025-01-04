typedef struct
{
    Tick_Input qbuf[NET_MAX_INPUT_TICKS];
    RngU64 playback_range;
    Tick_Input last_input;
    Uint64 latest_client_tick_id;
} Server_PlayerInputBuffer;

typedef struct
{
    Net_User users[NET_MAX_PLAYERS];
    Uint32 user_count;
    Object_Key player_keys[NET_MAX_PLAYERS];
    Server_PlayerInputBuffer player_inputs[NET_MAX_PLAYERS];
} Server_State;
