typedef struct
{
    SDL_Texture *tex;
    Uint32 tex_frames;
    CollisionVertices collision_vertices;
    CollisionNormals collision_normals;
} Sprite;

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
    LogFlags_NetAll      = (0xff << 1),
} Log_Flags;

// @note disable logging
//#define LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ SDL_Log(__VA_ARGS__); }}while(0)

// @note disable logging but use printf for compiler checks
//#define LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ printf(__VA_ARGS__); }}while(0)

// @note enable logging
#define LOG(FLAGS, ...) do{ if(((FLAGS) & APP.log_filter) == (FLAGS)){ SDL_Log(__VA_ARGS__); }}while(0)

struct AppState
{
    // SDL, window stuff
    SDL_Window* window;
    //SDL_Renderer* renderer;
    Gpu_State gpu;
    Rdr_State rdr;

    // window settings
    Sint32 init_window_px, init_window_py; // 0 if wasn't set
    Sint32 init_window_width, init_window_height;
    Sint32 window_width, window_height;
    bool window_on_top;
    bool window_borderless;

    bool window_autolayout;
    Uint32 latest_autolayout_user_count;

    Uint32 log_filter; // active log filer flags

    // time
    Uint64 frame_id;
    Uint64 frame_time;
    float dt;
    float at;
    Uint64 tick_id;
    float tick_dt_accumulator;

    // user input
    V2 mouse;
    V2 world_mouse;
    SDL_MouseButtonFlags mouse_keys;
    bool keyboard[SDL_SCANCODE_COUNT]; // true == key is down

    // networking
    struct
    {
        bool err; // tracks if network is in error state
        bool is_server;
        SDLNet_DatagramSocket *socket;

        Net_User server_user;

        // msg payload
        bool packet_err; // set on internal buffer overflow errors etc
        Net_PacketHeader packet_header;
        Uint8 packet_payload_buf[1024 * 1024 * 1]; // 1 MB scratch buffer for network payload construction
        Uint32 payload_used;
    } net;

    Client_State client;
    Server_State server;

    // objects
    union
    {
        struct
        {
            Object const_objects[OBJ_MAX_CONST_OBJECTS];
            Object net_objects[OBJ_MAX_NETWORK_OBJECTS];
        };
        Object all_objects[OBJ_MAX_ALL_OBJECTS];
    };

    // sprites
    Sprite sprite_pool[32];
    Uint32 sprite_count;
    Uint32 sprite_overlay_id;
    Uint32 sprite_dude_id; // @todo better organization

    // special objects
    Object_Key pathing_marker;
    bool pathing_marker_set;

    // camera
    V3 camera_p;
    V3 camera_rot;

    // debug
    struct
    {
        float fixed_dt;
        bool single_tick_stepping;
        bool unpause_one_tick;

        bool noclip_camera;

        bool draw_collision_box;
        float collision_sprite_animation_t;
        Uint32 collision_sprite_frame_index;

        bool draw_texture_box;
    } debug;
};

typedef struct
{
    Uint32 x, y, w, h;
} WindowLayoutData;

static void Game_AutoLayoutApply(AppState *app, Uint32 user_count, Sint32 px, Sint32 py, Sint32 w, Sint32 h);
static V2 Game_WorldToScreen(AppState *app, V2 pos);
static V2 Game_ScreenToWorld(AppState *app, V2 pos);


