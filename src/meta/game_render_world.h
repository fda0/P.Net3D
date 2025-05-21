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
  float material_roughness;
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
