#define MDL_MAX_INSTANCES 16

// Uniform for shader_world
typedef struct
{
  Mat4 camera_transform;
  Mat4 shadow_transform;
  _Alignas(16) V3 camera_position;
  _Alignas(16) V3 sun_dir;
  U32 flags;

  U32 fog_color; // RGBA
  U32 sky_ambient; // RGBA
  U32 sun_diffuse; // RGBA
  U32 sun_specular; // RGBA

  U32 material_diffuse; // RGBA
  U32 material_specular; // RGBA
  float material_shininess;
  float material_loaded_t;
} WORLD_Uniform;

// Vertices
typedef struct
{
  V3 p;
  V3 normal;
  V2 uv;
  U32 joints_packed4;
  V4 joint_weights;
} WORLD_Vertex;

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
