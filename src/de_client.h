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
    Client_Snapshot obj_snaps[OBJ_MAX_NETWORK_OBJECTS];
    Uint64 next_playback_tick;

    Uint16 current_playback_delay;
    TickDeltas playable_tick_deltas; // used to control playback catch-up

    // circular buffer with tick inputs
    Tick_Input inputs_qbuf[NET_MAX_INPUT_TICKS];
    RngU64 inputs_range;

    //
    Object_Key player_key;
    Uint64 player_key_latest_tick_id;
} Client_State;
