typedef enum
{
  RdrRigid_Teapot,
  RdrRigid_Flag,
  RdrRigid_COUNT
} Rdr_RigidType;

typedef struct
{
  Mat4 transform;
  V4 color;
} Rdr_RigidInstance;

typedef struct
{
  V3 p;
  V3 color;
  V3 normal;
} Rdr_RigidVertex;

typedef struct
{
  V3 p;
  V3 color;
  V3 normal;
  V3 uv;
} Rdr_WallVertex;

typedef struct
{
#define RDR_MAX_MODEL_INSTANCES 16
  Rdr_RigidInstance instances[RDR_MAX_MODEL_INSTANCES];
  U32 instance_count;
} Rdr_Rigid;

typedef struct
{
  Rdr_Rigid rigids[RdrRigid_COUNT];

  Rdr_WallVertex wall_verts[1024 * 8];
  U32 wall_vert_count;
} Rdr_State;
