typedef struct
{
    Tick_Input circle_inputs[NET_MAX_INPUT_TICKS];
    // playback
    Uint64 playback_index_min; // to-be-played index
    Uint64 playback_index_max; // one past past input from the server
    Uint64 latest_client_tick_id;
} Server_PlayerInputBuffer;

typedef struct
{
    Net_User users[NET_MAX_PLAYERS];
    Uint32 user_count;
    Object_Key player_keys[NET_MAX_PLAYERS];
    Server_PlayerInputBuffer player_inputs[NET_MAX_PLAYERS];
} Server_State;
