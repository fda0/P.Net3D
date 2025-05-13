typedef struct
{
  U32 color;

  U32 vertices_start_index;
  U32 indices_start_index;
  U32 indices_count;
} ASSET_Geometry;

typedef struct
{
  bool is_skinned;
  U32 skeleton_index;
  ASSET_Geometry *geos;
  U32 geos_count;
} ASSET_Model;

typedef struct
{
  U32 joint_index : 30;
  U32 type : 2; // PIE_TransformType
  U32 count;
  float *inputs; // 1 * count
  float *outputs; // (type == Rotation ? 4 : 3) * count
  // add interpolation type? assume linear for now
} ASSET_AnimationChannel;

typedef struct
{
  char *name; // @todo delete
  S8 name_s8;
  float t_min, t_max;
  ASSET_AnimationChannel *channels;
  U32 channels_count;
} ASSET_Animation;

typedef struct
{
  Mat4 root_transform;

  // --- animations ---
  ASSET_Animation *anims;
  U32 anims_count;

  // --- joints section ---
  // all pointers point to arrays with
  // joints_count number of elements
  U32 joints_count;
  const char **names; // @todo delete
  S8 *names_s8;
  Mat4 *inv_bind_mats;
  //
  U32 *child_index_buf;
  RngU32 *child_index_ranges;
  // rest pose translations, rotations, scales
  V3 *translations;
  Quat *rotations;
  V3 *scales;
} ASSET_Skeleton;

typedef struct
{
  U64 last_touched_frame;
  float loaded_t;
  U32 error : 1;
  U32 loaded : 1;
} ASSET_Base;

typedef struct
{
  ASSET_Base b;
  bool has_texture;
  SDL_GPUTexture *tex;

  U32 diffuse;
  U32 specular;
  float shininess;
} ASSET_Material;

typedef struct
{
  ASSET_Base b;

  SDL_GPUTexture *handle;
  float shininess;
} ASSET_Texture;

typedef struct
{
  Arena *arena;
  S8 file;
  PIE_Header *header;
  PIE_Links *links;

  PIE_Model *models;
  U32 models_count;

  PIE_Material *materials;
  U32 materials_count;
} ASSET_PieFile;

typedef struct
{
  ASSET_PieFile pie;

  // Texture
  SDL_GPUTexture *tex_fallback;
  ASSET_Texture textures[TEX_COUNT];
  // Texture thread
  bool tex_load_needed;
  SDL_Semaphore *tex_sem;

  // Skeleton
  ASSET_Skeleton skeletons[MODEL_COUNT];
  U32 skeletons_count;

  // Geometry
  SDL_GPUBuffer *rigid_vertices;
  SDL_GPUBuffer *skinned_vertices;
  SDL_GPUBuffer *indices;
  ASSET_Model models[MODEL_COUNT];

  U64 serialize_last_check_timestamp;
  U64 serialize_hash;
} ASSET_State;

static void PIE_LoadFile(const char *pie_file_path);
