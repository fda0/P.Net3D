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
    ObjectFlag_Draw            = (1 << 0),
    ObjectFlag_Move            = (1 << 1),
    ObjectFlag_Collide         = (1 << 2),
    ObjectFlag_AnimateRotation = (1 << 3),
    ObjectFlag_AnimatePosition = (1 << 4),
    ObjectFlag_ModelTeapot     = (1 << 5),
    ObjectFlag_ModelFlag       = (1 << 6),
} Object_Flags;

typedef enum
{
    ObjCategory_Local = (1 << 0),
    ObjCategory_Net   = (1 << 1),

    ObjCategory_All = (ObjCategory_Local | ObjCategory_Net)
} Obj_Category;

typedef struct
{
    U32 made_at_tick;
    U32 index;
} Object_Key;

typedef struct
{
    Object_Key key;
    U32 flags;
    bool init;
    V2 p; // position of center
    V2 dp; // change of p
    V2 prev_p; // position from the last frame

    // visuals
    ColorF color;
    float rot_z;
    //float animated_rot_z; // animates towards model_rot_z
    Quat animated_rot;
    V3 animated_p; // animates towards (V3){p.x, p.y, 0}

    // input actions
    bool is_pathing;
    V2 pathing_dest_p;

    Collision_Data collision;

    // temp
    U32 some_number;
    bool did_collide;
} Object;

static Object Object_Lerp(Object prev, Object next, float t);
static Object *Object_CreatePlayer();
static void Collision_RecalculateNormals(Collision_Data *collision);
