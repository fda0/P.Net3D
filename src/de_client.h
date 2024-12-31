typedef struct
{
    Object tick_states[NET_CLIENT_MAX_SNAPSHOTS]; // circle buf
    Uint64 latest_server_tick;
    Uint64 oldest_server_tick;
    Uint64 recent_lerp_start_tick;
    Uint64 recent_lerp_end_tick;
} Client_Snapshot;

typedef struct
{
    Client_Snapshot obj_snaps[NET_MAX_NETWORK_OBJECTS];
    Uint64 next_playback_tick;
    Uint64 current_playback_delay;
    
    // stores positive deltas from latest; used to speed up playback and catch up to server on stable connections
    Uint64 prev_smallest_latest_server_tick;
    Sint16 latest_deltas[32];
    Sint16 latest_delta_index;
    Sint16 playback_tick_catchup;
    
    // circular buffer with tick inputs
    Tick_Input circle_inputs[NET_MAX_INPUT_TICKS];
    Uint64 tick_input_min;
    Uint64 tick_input_max; // one past last
} Client_State;
