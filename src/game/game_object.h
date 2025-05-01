#define OBJ_MAX_NETWORK_OBJECTS 24
#define OBJ_MAX_OFFLINE_OBJECTS 512
#define OBJ_MAX_ALL_OBJECTS (OBJ_MAX_NETWORK_OBJECTS + OBJ_MAX_OFFLINE_OBJECTS)

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

  ObjFlag_DrawModel       = (1 << 5),
  ObjFlag_DrawCollision   = (1 << 6),
} OBJ_Flags;

typedef enum
{
  // This is a flag so it can be used for querying multiple types of Objects.
  // A single object can have only one storage type.
  OBJ_Offline = (1 << 0),
  OBJ_Network = (1 << 1),
  OBJ_StorageAll = (OBJ_Offline | OBJ_Network)
} OBJ_Storage;

typedef struct
{
  U32 serial_number;
  U32 index;
} OBJ_Key;

typedef struct
{
  // Object data that's synced with the server
  OBJ_Key key;
  U32 flags;
  bool init;
  V3 p; // XY center, Z at bottom
  V3 desired_dp; // move that Object wanted to make
  V3 moved_dp; // move that Object did after collision simulation

  // visuals
  U32 color;
  Quat rotation;
  U32 animation_index; // used by DrawModel
  MODEL_Type model; // used by DrawModel
  float collision_height; // used by DrawCollision
  TEX_Kind texture; // used by DrawCollision
  float texture_texels_per_cm; // used by DrawCollision

  // input actions
  bool is_pathing;
  V2 pathing_dest_p;

  Collision_Data collision;
} OBJ_Sync;

typedef struct
{
  // Object data that's kept on client side only
  Quat animated_rot; // animates towards rotation
  V3 animated_p; // animates towards (V3){p.x, p.y, p.z}
  float animation_t;
} OBJ_Local;

typedef struct
{
  OBJ_Sync s;
  OBJ_Local l;
} Object;

static OBJ_Sync OBJ_SyncLerp(OBJ_Sync prev, OBJ_Sync next, float t);
static Object *OBJ_CreatePlayer();
static void Collision_RecalculateNormals(Collision_Data *collision);
