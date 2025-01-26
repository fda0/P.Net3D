#define OBJ_MAX_NETWORK_OBJECTS 24
#define OBJ_MAX_CONST_OBJECTS 128
#define OBJ_MAX_ALL_OBJECTS (OBJ_MAX_NETWORK_OBJECTS + OBJ_MAX_CONST_OBJECTS)

typedef struct
{
    CollisionVertices verts;
    CollisionNormals norms;
} Collision_Data;

typedef enum
{
    ObjectFlag_Draw          = (1 << 0),
    ObjectFlag_Move          = (1 << 1),
    ObjectFlag_Collide       = (1 << 2),
    ObjectFlag_RenderTeapot  = (1 << 3),
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
    ColorF sprite_color;
    Uint32 some_number;

    // input actions
    bool is_pathing;
    V2 pathing_dest_p;

    Collision_Data collision;

    // temp
    bool did_collide;
} Object;

static Object Object_Lerp(Object prev, Object next, float t);
static Object *Object_CreatePlayer(AppState *app);
static void Collision_RecalculateNormals(Collision_Data *collision);
