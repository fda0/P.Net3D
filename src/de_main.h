// ---
// Constants
// ---
#define TICK_RATE 32
#define TIME_STEP (1.f / (float)TICK_RATE)

#define NET_DEFAULT_SEVER_PORT 21037
#define NET_MAGIC_VALUE 0xfda0'dead'beef'1234llu
#define NET_MAX_TICK_HISTORY (TICK_RATE)
#define NET_MAX_NETWORK_OBJECTS 16
#define NET_MAX_MSG_SIZE 32768
#define NET_OLD_PROTOCOL 0
#define NET_SIMULATE_PACKETLOSS 0 // doesn't seem to work on localhost

typedef struct
{
    SDL_Texture *tex;
    Uint32 tex_frames;
    Col_Vertices collision_vertices;
    Col_Normals collision_normals;
} Sprite;

typedef enum {
    ObjectFlag_Draw          = (1 << 0),
    ObjectFlag_Move          = (1 << 1),
    ObjectFlag_Collide       = (1 << 2),
} Object_Flags;

typedef struct
{
    Uint32 flags;
    V2 p; // position of center
    V2 dp; // change of p
    V2 prev_p; // position from the last frame

    // visuals
    Uint32 sprite_id;
    ColorF sprite_color;

    float sprite_animation_t;
    Uint32 sprite_animation_index;
    Uint32 sprite_frame_index;

    // temp
    bool has_collision;
} Object;

typedef struct
{
    SDLNet_Address *address;
    Uint16 port;
} Net_User;

typedef enum
{
    Tick_Cmd_None,
    Tick_Cmd_Input,
    Tick_Cmd_NetworkObj,
    Tick_Cmd_ObjHistory,
} Tick_CommandKind;

typedef struct
{
    Uint64 tick_id;
    Tick_CommandKind kind;
} Tick_Command;

typedef struct
{
    V2 move_dir;
    // action buttons etc will be added here
} Tick_Input;

typedef struct
{
    Object obj;
    Uint32 network_slot;
} Tick_NetworkObj;

typedef struct
{
    Object objs[NET_MAX_NETWORK_OBJECTS];
} Tick_NetworkObjState;

typedef struct
{
    Tick_NetworkObjState states[NET_MAX_TICK_HISTORY];
} Tick_NetworkObjHistory;

typedef struct
{
    // SDL, window stuff
    SDL_Window* window;
    SDL_Renderer* renderer;
    int window_width, window_height;
    int window_px, window_py; // initial window px, py specified by cmd options, 0 if wasn't set
    bool window_on_top;
    bool window_borderless;

    // user input
    V2 mouse;
    SDL_MouseButtonFlags mouse_keys;
    bool keyboard[SDL_SCANCODE_COUNT]; // true == key is down

    // circular buffer with tick inputs
    Tick_Input tick_input_buf[NET_MAX_TICK_HISTORY];
    Uint64 tick_input_min;
    Uint64 tick_input_max; // one past last

    struct
    {
        Tick_NetworkObjState states[NET_MAX_TICK_HISTORY];
        Uint64 next_tick;
        Uint64 latest_server_at_tick;

        // stores last 32 tick bumps (how much newer the server's tick was compared to our previous latest server tick) to adjust network delay
        Uint64 tick_bump_history[32]; // size of this should have inv corelation to our network poll rate
        Uint64 tick_bump_history_next;
        Uint64 tick_bump_correction;
    } netobj;

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
        bool send_err; // set on internal buffer overflow errors etc
        Uint8 payload_buf[1024 * 1024 * 1]; // 1 MB scratch buffer for network payload construction
        Uint32 payload_buf_used;
        Uint8 packet_buf[1024 * 1024 * 1]; // 1 MB scratch buffer for network packet construction
        S8 packet_slices[16]; // support 16 parts max
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
} AppState;
