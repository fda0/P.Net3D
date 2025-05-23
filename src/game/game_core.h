typedef enum
{
  Log_Idk         = (1 << 0),
  Log_NetInfo     = (1 << 1),
  Log_NetDatagram = (1 << 2),
  Log_NetSend     = (1 << 3),
  Log_NetPacket   = (1 << 4),
  Log_NetPayload  = (1 << 5),
  Log_NetClient   = (1 << 6),
  Log_NetTick     = (1 << 7),
  Log_NetCatchup  = (1 << 8),
  Log_NetAll      = (0xff << 1),

  Log_Debug = (1 << 9),
  Log_GPU = (1 << 10),
  Log_Perf = (1 << 11),
  Log_Clay = (1 << 12),
  Log_OS = (1 << 13),
  Log_Serial = (1 << 14),
} Log_Flags;

// @note disable logging
//#define LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ SDL_Log(__VA_ARGS__); }}while(0)

// @note disable logging but use printf for compiler checks
//#define LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ printf(__VA_ARGS__); }}while(0)

// @note enable logging
#define LOG(FLAGS, ...) do{ if(((FLAGS) & APP.log_filter) == (FLAGS)){ SDL_Log(__VA_ARGS__); }}while(0)

typedef enum
{
  WorldDir_E, // +X
  WorldDir_W, // -X
  WorldDir_N, // +Y
  WorldDir_S, // -Y
  WorldDir_T, // +Z
  WorldDir_B, // -Z
  WorldDir_COUNT
} WorldDir;

typedef enum
{
  KEY_START = SDL_SCANCODE_COUNT + 1,

  KEY_MouseLeft,
  KEY_MouseRight,

  KEY_COUNT
} KeyCode;

typedef struct
{
  U8 held : 1;
  U8 pressed : 1; // was pressed on this frame
  U8 released : 1; // released on this frame
} Key;

struct AppState
{
  // SDL, window stuff
  SDL_Window* window;
  float dpi_scaling;
  bool in_shutdown;

  bool fullscreen;
  bool headless; // no window mode
  GPU_State gpu;
  FONT_State font;
  AST_State ast;

  Arena *tmp;
  Arena *a_frame;

  // window settings
  I32 init_window_px, init_window_py; // 0 if wasn't set
  I32 init_window_width, init_window_height;
  I32 window_width, window_height;
  V2 window_dim;
  bool window_resized;
  bool window_on_top;
  bool window_borderless;

  bool window_autolayout;
  U32 latest_autolayout_user_count;

  U32 log_filter; // active log filer flags

  // special objects
  OBJ_Key pathing_marker;
  bool pathing_marker_set;

  // sun
  OBJ_Key sun;
  V3 towards_sun_dir; // normalized
  V3 sun_camera_p;
  Mat4 sun_camera_transform;

  // camera
  V3 camera_p;
  V3 camera_angles;
  float camera_fov_y;
  Mat4 camera_transform;

  // time
  U64 timestamp; // ms
  U64 tick_timestamp_accumulator;
  float dt;
  float at;
  U64 frame_id;
  U64 tick_id;
  U32 obj_serial_counter;

  // time stats
  U64 timestamp_app_launch;
  U64 timestamp_post_first_frame;

  // user input
  V2 mouse;
  V2 mouse_delta;
  V2 mouse_scroll;
  bool world_mouse_valid;
  V2 world_mouse;
  Key keys[KEY_COUNT];

  // objects
  union
  {
    struct
    {
      Object offline_objects[OBJ_MAX_OFFLINE_OBJECTS];
      Object net_objects[OBJ_MAX_NETWORK_OBJECTS];
    };
    Object all_objects[OBJ_MAX_ALL_OBJECTS];
  };

  // networking
  struct
  {
    bool err; // tracks if network is in error state
    bool is_server;
    SDLNet_DatagramSocket *socket;

    NET_User server_user;

    // msg payload
    bool packet_err; // set on internal buffer overflow errors etc
    NET_PacketHeader packet_header;
    U8 packet_payload_buf[1024 * 1024 * 1]; // 1 MB scratch buffer for network payload construction
    U32 payload_used;
  } net;

  CLIENT_State client;
  SERVER_State server;

  // debug
  struct
  {
    U64 serialize_last_check_timestamp;
    U64 serialize_hash;

    bool show_debug_window;

    U32 click_id;
    V2 win_p;
    U32 menu_category;

    float tex_displacement;

    float fixed_dt;
    bool single_tick_stepping;
    bool unpause_one_tick;

    bool noclip_camera;
    bool sun_camera;

    bool draw_model_collision;
  } debug;
};

typedef struct
{
  U32 x, y, w, h;
} WindowLayoutData;

static void Game_AutoLayoutApply(U32 user_count, I32 px, I32 py, I32 w, I32 h);


