#define MDL_MAX_INSTANCES 16

// Uniform for shader_world
typedef struct
{
  Mat4 camera_transform;
  Mat4 shadow_transform;
  _Alignas(16) V3 camera_position;
  _Alignas(16) V3 background_color;
  _Alignas(16) V3 towards_sun_dir;
  float tex_loaded_t;
  float tex_shininess;
} WORLD_Uniform;

// Vertices
typedef struct
{
  V3 normal;
  V3 p;
  U32 color;
} WORLD_VertexRigid;

typedef struct
{
  V3 normal;
  V3 p;
  U32 color;
  U32 joints_packed4;
  V4 weights;
} WORLD_VertexSkinned;

typedef struct
{
  V3 normal;
  V3 p;
  V2 uv;
  U32 color;
} WORLD_VertexMesh;

// Instances
typedef struct
{
  Mat4 transform;
  U32 color;
  U32 pose_offset; // in indices; unused for rigid
} WORLD_InstanceModel;

// UI - User Interface
typedef struct
{
  V2 window_dim;
  V2 texture_dim;
} UI_Uniform;

typedef struct
{
  V2 p_min;
  V2 p_max;
  V2 tex_min;
  V2 tex_max;
  float tex_layer;
  float corner_radius;
  float edge_softness;
  float border_thickness;
  U32 color; // @todo array4
} UI_Shape;

typedef struct
{
  V2 p_min;
  V2 p_max;
} UI_Clip;

static void GPU_PostFrameClear();
