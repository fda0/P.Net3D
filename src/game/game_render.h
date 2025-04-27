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
} World_GpuUniform;

// Vertices
typedef struct
{
  Quat normal_rot;
  V3 p;
  U32 color;
} MDL_GpuRigidVertex;

typedef struct
{
  Quat normal_rot;
  V3 p;
  U32 color;
  U32 joints_packed4;
  V4 weights;
} MDL_GpuSkinnedVertex;

typedef struct
{
  Mat4 transform;
  U32 color;
  U32 pose_offset; // in indices; unused for rigid
} MDL_GpuInstance;

#if 0
typedef struct
{
  MDL_GpuInstance instances[MDL_MAX_INSTANCES];
  U32 instances_count;

  struct
  {
    SDL_GPUBuffer *vertices;
    SDL_GPUBuffer *indices;
    SDL_GPUBuffer *instances;
    U32 indices_count;
  } gpu;
} MDL_Batch;
#endif

// MSH - Mesh
typedef struct
{
  Quat normal_rot;
  V3 p;
  V2 uv;
  U32 color;
} MSH_GpuVertex;

#if 0
typedef struct
{
  MSH_GpuVertex vertices[1024 * 8];
  U32 vertices_count;
  SDL_GPUBuffer *gpu_vertices;
} MSH_Batch;
#endif

// UI - User Interface
typedef struct
{
  V2 window_dim;
  V2 texture_dim;
} UI_GpuUniform;

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
} UI_GpuShape;

typedef struct
{
  V2 p_min;
  V2 p_max;
} UI_GpuClip;

static void GPU_ClearBuffers();
