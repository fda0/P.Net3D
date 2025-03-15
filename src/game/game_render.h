#define RD_MAX_RIGID_INSTANCES 16
#define RD_MAX_SKINNED_INSTANCES 16


typedef enum
{
  TEX_Bricks071,
  TEX_Bricks097,
  TEX_Grass004,
  TEX_Ground037,
  TEX_Ground068,
  TEX_Ground078,
  TEX_Leather011,
  TEX_PavingStones067,
  TEX_Tiles101,
  TEX_TestPBR001,
  TEX_COUNT
} TEX_Kind;


//
// Enums @todo generate these or setup more dynamic key-like system
//
typedef enum
{
  RdrRigid_Teapot,
  RdrRigid_Flag,
  RdrRigid_COUNT
} RDR_RigidType;

typedef enum
{
  RdrSkinned_Worker,
  RdrSkinned_COUNT
} RDR_SkinnedType;

// Uniform
typedef struct
{
  Mat4 camera_transform;
  Mat4 shadow_transform;
  _Alignas(16) V3 camera_position;
  _Alignas(16) V3 background_color;
  _Alignas(16) V3 towards_sun_dir;
} RDR_Uniform;

// Vertices
typedef struct
{
  Quat normal_rot;
  V3 p;
  U32 color;
} RDR_RigidVertex;

typedef struct
{
  Quat normal_rot;
  V3 p;
  U32 color;
  U32 joints_packed4;
  V4 weights;
} RDR_SkinnedVertex;

typedef struct
{
  Quat normal_rot;
  V3 p;
  V2 uv;
  U32 color;
} RDR_WallVertex;

// Instance buffers
typedef struct
{
  Mat4 transform;
  U32 color;
} RDR_RigidInstance;

typedef struct
{
  Mat4 transform;
  U32 color;
  U32 pose_offset;
} RDR_SkinnedInstance;

// Pose buffer
typedef struct
{
  Mat4 mats[62];
} RDR_SkinnedPose;

// Structs that hold things
typedef struct
{
  RDR_RigidInstance instances[RD_MAX_RIGID_INSTANCES];
  U32 instance_count;
} RDR_Rigid;

typedef struct
{
  RDR_SkinnedInstance instances[RD_MAX_SKINNED_INSTANCES];
  RDR_SkinnedPose poses[RD_MAX_SKINNED_INSTANCES];
  U32 instance_count;
} RDR_Skinned;

typedef struct
{
  RDR_WallVertex verts[1024 * 8];
  U32 vert_count;
} RDR_WallMeshBuffer;

// State
typedef struct
{
  RDR_RigidInstance rigid_instances[RD_MAX_RIGID_INSTANCES];
  U32 rigid_instances_count;
  RDR_SkinnedInstance skinned_instances[RD_MAX_RIGID_INSTANCES];
  U32 skinned_instances_count;

  RDR_Rigid rigids[RdrRigid_COUNT];
  RDR_Skinned skinneds[RdrSkinned_COUNT];

  RDR_WallMeshBuffer wall_mesh_buffers[TEX_COUNT];
} RDR_State;
