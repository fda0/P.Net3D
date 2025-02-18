#define RDR_MAX_MODEL_INSTANCES 16

//
// Enums @todo generate these or setup more dynamic key-like system
//
typedef enum
{
  RdrRigid_Teapot,
  RdrRigid_Flag,
  RdrRigid_COUNT
} Rdr_RigidType;

typedef enum
{
  RdrSkinned_Worker,
  RdrSkinned_COUNT
} Rdr_SkinnedType;

typedef enum
{
  RdrModel_Rigid_Teapot,
  RdrModel_Rigid_Flag,
  RdrModel_Skinned_Worker,
  RdrModel_COUNT
} Rdr_ModelType;

//
// Rigid models
//
typedef struct
{
  V3 p;
  V3 color;
  V3 normal;
} Rdr_RigidVertex;

typedef struct
{
  Mat4 transform;
  V4 color;
} Rdr_RigidInstance;

typedef struct
{
  Rdr_RigidInstance instances[RDR_MAX_MODEL_INSTANCES];
  U32 instance_count;
} Rdr_Rigid;

//
// Skinned models
//
typedef struct
{
  V3 p;
  V3 color;
  V3 normal;
  U32 joints_packed4;
  V4 weights;
} Rdr_SkinnedVertex;

typedef struct
{
  Mat4 transform;
  V4 color;
} Rdr_SkinnedInstance;

typedef struct
{
  Rdr_SkinnedInstance instances[RDR_MAX_MODEL_INSTANCES];
  U32 instance_count;
} Rdr_Skinned;

//
// Wall models
//
typedef struct
{
  V3 p;
  V3 color;
  V3 normal;
  V3 uv;
} Rdr_WallVertex;

//
//
//
typedef struct
{
  Rdr_Rigid rigids[RdrRigid_COUNT];
  Rdr_Skinned skinneds[RdrSkinned_COUNT];

  Rdr_WallVertex wall_verts[1024 * 8];
  U32 wall_vert_count;
} Rdr_State;
