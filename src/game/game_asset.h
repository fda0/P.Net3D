typedef struct
{
  MATERIAL_Key material;

  U32 vertices_start_index;
  U32 indices_start_index;
  U32 indices_count;
} ASSET_Mesh;

typedef struct
{
  MODEL_Key key;
  bool is_skinned;
  U32 skeleton_index;
  ASSET_Mesh *meshes;
  U32 meshes_count;
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
  S8 name;
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
  MATERIAL_Key key;

  PIE_MaterialParams params;
  bool has_texture;
  U32 texture_layers;
  SDL_GPUTexture *tex;
} ASSET_Material;

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
  ASSET_Material nil_material;
  ASSET_Material *materials;
  U32 materials_count;

  // Texture thread
  bool tex_load_needed;
  SDL_Semaphore *tex_sem;

  // Meshes
  ASSET_Model nil_model;
  ASSET_Model *models;
  U32 models_count;

  SDL_GPUBuffer *vertices;
  SDL_GPUBuffer *indices;

  // Skeleton
  ASSET_Skeleton *skeletons;
  U32 skeletons_count;

  U64 serialize_last_check_timestamp;
  U64 serialize_hash;
} ASSET_State;

static void ASSET_LoadPieFile(const char *pie_file_path);
