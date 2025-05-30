#define OBJ_MAX_NETWORK_OBJECTS 24
#define OBJ_MAX_OFFLINE_OBJECTS 512
#define OBJ_MAX_ALL_OBJECTS (OBJ_MAX_NETWORK_OBJECTS + OBJ_MAX_OFFLINE_OBJECTS)
#define OBJ_MAX_ANIMATIONS 4

typedef enum
{
  ObjFlag_Move            = (1 << 0),
  ObjFlag_Collide         = (1 << 1),

  ObjFlag_AnimateRotation = (1 << 2),
  ObjFlag_AnimatePosition = (1 << 3),
  ObjFlag_AnimateTracks   = (1 << 4),

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

#define OBJ_COLLIDER_MAX_VERTS 4
typedef struct
{
  V2 vals[OBJ_COLLIDER_MAX_VERTS];
} OBJ_Collider;

typedef struct
{
  RngF ranges[OBJ_COLLIDER_MAX_VERTS];
} OBJ_ColliderProjection;

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
  MODEL_Key model; // used by DrawModel
  MATERIAL_Key material; // used by DrawCollision
  float height; // used by DrawCollision
  float texture_texels_per_m; // used by DrawCollision

  // input actions
  bool is_pathing;
  V2 pathing_dest_p;

  OBJ_Collider collider_vertices;
} OBJ_Sync;

typedef struct
{
  // Object data that's kept on client side only
  Quat animated_rot; // animates towards rotation
  V3 animated_p; // animates towards (V3){p.x, p.y, p.z}

  U32 animation_index;
  float animation_t;

  bool collider_normals_are_updated;
  OBJ_Collider collider_normals;
} OBJ_Local;

typedef struct
{
  OBJ_Sync s;
  OBJ_Local l;
} Object;

static OBJ_Sync OBJ_SyncLerp(OBJ_Sync prev, OBJ_Sync next, float t);
