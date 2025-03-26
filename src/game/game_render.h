#define MDL_MAX_INSTANCES 16

// Uniform for shader_world
typedef struct
{
  Mat4 camera_transform;
  Mat4 shadow_transform;
  _Alignas(16) V3 camera_position;
  _Alignas(16) V3 background_color;
  _Alignas(16) V3 towards_sun_dir;
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
  U32 pose_offset; // unused for rigid
} MDL_GpuInstance;

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

// MSH - Mesh
typedef struct
{
  Quat normal_rot;
  V3 p;
  V2 uv;
  U32 color;
} MSH_GpuVertex;

typedef struct
{
  MSH_GpuVertex verts[1024 * 8];
  U32 vert_count;

  struct
  {
    SDL_GPUTexture *texture;
    SDL_GPUBuffer *vertices;
  } gpu;
} MSH_Batch;

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
  U32 color;
  U32 border_color;
  float border_radius[4];
} UI_GpuShape;

typedef struct
{
  V2 p_min;
  V2 p_max;
} UI_GpuClip;
