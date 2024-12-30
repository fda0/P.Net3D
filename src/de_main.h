// ---
// Constants
// ---
#define TICK_RATE 120
#define TIME_STEP (1.f / (float)TICK_RATE)

typedef struct
{
    SDL_Texture *tex;
    Uint32 tex_frames;
    Col_Vertices collision_vertices;
    Col_Normals collision_normals;
} Sprite;

typedef struct
{
    SDLNet_Address *address;
    Uint16 port;
} Net_User;

typedef enum
{
    NetCmd_None,
    NetCmd_Ping,
    NetCmd_ObjUpdate,
    NetCmd_ObjEmpty,
    NetCmd_NetworkTest,
} Net_CmdKind;

typedef struct
{
    Uint64 tick_id; // this is already send via packet header
    Net_CmdKind kind;
} Net_Cmd;

typedef struct
{
    V2 move_dir;
    // action buttons etc will be added here
} Tick_Input;

typedef struct
{
    Uint64 number;
} Tick_Ping;

typedef struct
{
    Uint32 net_slot;
    Object obj;
} Net_ObjUpdate;

typedef struct
{
    Uint32 net_slot;
} Net_ObjEmpty;

typedef struct
{
    Uint32 numbers[290];
} Net_Payload_NetworkTest;

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
} ClientState;

typedef struct
{
    Object objs[NET_MAX_NETWORK_OBJECTS];
} ServerTickSnapshot;

typedef struct
{
    ServerTickSnapshot snaps[NET_MAX_TICK_HISTORY]; // circle buffer
    Uint64 next_tick;
} ServerState;

typedef enum
{
    LogFlags_Idk         = (1 << 0),
    LogFlags_NetInfo     = (1 << 1),
    LogFlags_NetDatagram = (1 << 2),
    LogFlags_NetSend     = (1 << 3),
    LogFlags_NetPacket   = (1 << 4),
    LogFlags_NetPayload  = (1 << 5),
    LogFlags_NetClient   = (1 << 6),
    LogFlags_NetTick     = (1 << 7),
    LogFlags_NetCatchup  = (1 << 8),
} Log_Flags;

// @note disable logging
//#define LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ SDL_Log(__VA_ARGS__); }}while(0)

// @note disable logging but use printf for compiler checks
//#define LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ printf(__VA_ARGS__); }}while(0)

// @note enable logging
#define LOG(FLAGS, ...) do{ if(((FLAGS) & app->log_filter) == (FLAGS)){ SDL_Log(__VA_ARGS__); }}while(0)

struct AppState
{
    // SDL, window stuff
    SDL_Window* window;
    SDL_Renderer* renderer;
    int window_width, window_height;
    int window_px, window_py; // initial window px, py specified by cmd options, 0 if wasn't set
    bool window_on_top;
    bool window_borderless;

    Uint32 log_filter; // active log filer flags

    // user input
    V2 mouse;
    SDL_MouseButtonFlags mouse_keys;
    bool keyboard[SDL_SCANCODE_COUNT]; // true == key is down

    // circular buffer with tick inputs
    Tick_Input tick_input_buf[NET_MAX_TICK_HISTORY];
    Uint64 tick_input_min;
    Uint64 tick_input_max; // one past last

    ClientState client;
    ServerState server;

    // time
    Uint64 frame_id;
    Uint64 frame_time;
    float dt;
    Uint64 tick_id;
    float tick_dt_accumulator;

    // objects
    Object object_pool[4096];
    Uint32 object_count;
    Uint32 network_ids[NET_MAX_NETWORK_OBJECTS];
    Uint32 player_network_slot;

    // sprites
    Sprite sprite_pool[32];
    Uint32 sprite_count;
    Uint32 sprite_overlay_id;
    Uint32 sprite_dude_id; // @todo better organization

    // camera
    V2 camera_p;
    // :: camera_range ::
    // how much of the world is visible in the camera
    // float camera_scale = Max(width, height) / camera_range
    float camera_range;

    // networking
    struct
    {
        bool err; // tracks if network is in error state
        bool is_server;
        SDLNet_DatagramSocket *socket;

        Net_User users[16];
        Uint32 user_count;
        Net_User server_user;

        // msg payload
        bool packet_err; // set on internal buffer overflow errors etc
        Net_PacketHeader packet_header;
        Uint8 packet_payload_buf[1024 * 1024 * 1]; // 1 MB scratch buffer for network payload construction
        Uint32 payload_used;
    } net;

    // debug
    struct
    {
        float fixed_dt;
        bool single_tick_stepping;
        bool unpause_one_tick;

        bool draw_collision_box;
        float collision_sprite_animation_t;
        Uint32 collision_sprite_frame_index;

        bool draw_texture_box;
    } debug;
};
