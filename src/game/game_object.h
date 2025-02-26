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
  ObjFlag_Move            = (1 << 0),
  ObjFlag_Collide         = (1 << 1),

  ObjFlag_AnimateRotation = (1 << 2),
  ObjFlag_AnimatePosition = (1 << 3),
  ObjFlag_AnimateT        = (1 << 4),

  ObjFlag_DrawTeapot          = (1 << 5),
  ObjFlag_DrawFlag            = (1 << 6),
  ObjFlag_DrawWorker          = (1 << 7),
  ObjFlag_DrawCollisionBrick  = (1 << 8),
  ObjFlag_DrawCollisionCobble = (1 << 9),
} Obj_Flags;

typedef enum
{
  ObjStorage_Local = (1 << 0),
  ObjStorage_Net   = (1 << 1),
  ObjStorage_All = (ObjStorage_Local | ObjStorage_Net)
} Obj_Category;

typedef struct
{
  U32 serial_number;
  U32 index;
} Obj_Key;

typedef struct
{
  // Object data that's synced with the server
  Obj_Key key;
  U32 flags;
  bool init;
  V2 p; // position of center
  V2 dp; // change of p
  V2 prev_p; // position from the last frame

  // visuals
  U32 color;
  bool hide_above_map;
  U32 animation_index;
  Quat rotation;

  // input actions
  bool is_pathing;
  V2 pathing_dest_p;

  Collision_Data collision;
} Obj_Sync;

typedef struct
{
  // Object data that's kept on client side only
  Quat animated_rot; // animates towards rotation
  V3 animated_p; // animates towards (V3){p.x, p.y, 0}
  float animation_t;
} Obj_Local;

typedef struct
{
  Obj_Sync s;
  Obj_Local l;
} Object;

static Obj_Sync Obj_SyncLerp(Obj_Sync prev, Obj_Sync next, float t);
static Object *Obj_CreatePlayer();
static void Collision_RecalculateNormals(Collision_Data *collision);
