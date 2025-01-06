#define OBJ_MAX_NETWORK_OBJECTS 24
#define OBJ_MAX_CONST_OBJECTS 128
#define OBJ_MAX_ALL_OBJECTS (OBJ_MAX_NETWORK_OBJECTS + OBJ_MAX_CONST_OBJECTS)

typedef enum
{
    ObjectFlag_Draw          = (1 << 0),
    ObjectFlag_Move          = (1 << 1),
    ObjectFlag_Collide       = (1 << 2),
} Object_Flags;

typedef enum
{
    ObjCategory_Local = (1 << 0),
    ObjCategory_Net   = (1 << 1),

    ObjCategory_All = (ObjCategory_Local | ObjCategory_Net)
} Obj_Category;

typedef struct
{
    Uint32 made_at_tick;
    Uint32 index;
} Object_Key;

typedef struct
{
    Object_Key key;
    Uint32 flags;
    bool init;
    V2 p; // position of center
    V2 dp; // change of p
    V2 prev_p; // position from the last frame

    // visuals
    Uint32 sprite_id;
    ColorF sprite_color;

    float sprite_animation_t;
    Uint32 sprite_animation_index;
    Uint32 sprite_frame_index;

    Uint32 some_number;

    // input actions
    bool is_pathing;
    V2 pathing_dest_p;

    // temp
    bool has_collision;
} Object;

static Object Object_Lerp(Object prev, Object next, float t);
static Object *Object_CreatePlayer(AppState *app);
