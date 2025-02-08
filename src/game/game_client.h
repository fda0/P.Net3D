typedef struct
{
    Obj_Sync tick_states[NET_CLIENT_MAX_SNAPSHOTS]; // circle buf
    U64 latest_server_tick;
    U64 oldest_server_tick;
    U64 recent_lerp_start_tick;
    U64 recent_lerp_end_tick;
} Client_ObjSnapshots;

typedef struct
{
    Client_ObjSnapshots snaps_of_objs[OBJ_MAX_NETWORK_OBJECTS];
    U64 next_playback_tick;

    U16 current_playback_delay;
    TickDeltas playable_tick_deltas; // used to control playback catch-up

    // circular buffer with tick inputs
    Tick_Input inputs_qbuf[NET_MAX_INPUT_TICKS];
    RngU64 inputs_range;

    //
    Obj_Key player_key;
    U64 player_key_latest_tick_id;
} Client_State;
