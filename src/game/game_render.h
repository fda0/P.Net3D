#define RD_MAX_RIGID_INSTANCES 16
#define RD_MAX_SKINNED_INSTANCES 16

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
  V3 normal;
  U32 color;
} Rdr_RigidVertex;

typedef struct
{
  Mat4 transform;
  U32 color;
} Rdr_RigidInstance;

typedef struct
{
  Rdr_RigidInstance instances[RD_MAX_RIGID_INSTANCES];
  U32 instance_count;
} Rdr_Rigid;

//
// Skinned models
//
typedef struct
{
  V3 p;
  V3 normal;
  U32 color;
  U32 joints_packed4;
  V4 weights;
} Rdr_SkinnedVertex;

typedef struct
{
  Mat4 transform;
  U32 color;
  U32 pose_offset;
} Rdr_SkinnedInstance;

typedef struct
{
  Mat4 mats[62];
} Rdr_SkinnedPose;

typedef struct
{
  Rdr_SkinnedInstance instances[RD_MAX_SKINNED_INSTANCES];
  Rdr_SkinnedPose poses[RD_MAX_SKINNED_INSTANCES];
  U32 instance_count;
} Rdr_Skinned;

//
// Wall models
//
typedef struct
{
  V3 p;
  V3 normal;
  V3 uv;
  U32 color;
} Rdr_WallVertex;

//
//
//
typedef struct
{
  Rdr_RigidInstance rigid_instances[RD_MAX_RIGID_INSTANCES];
  U32 rigid_instances_count;
  Rdr_SkinnedInstance skinned_instances[RD_MAX_RIGID_INSTANCES];
  U32 skinned_instances_count;

  Rdr_Rigid rigids[RdrRigid_COUNT];
  Rdr_Skinned skinneds[RdrSkinned_COUNT];

  Rdr_WallVertex wall_verts[1024 * 8];
  U32 wall_vert_count;
} Rdr_State;
