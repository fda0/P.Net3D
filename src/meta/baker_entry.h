typedef struct
{
  Printer file;
  bool finalized;

  Printer vertices;
  Printer indices;

  Printer models;
  U32 models_count;

  Printer skeletons;
  U32 skeletons_count;

  Printer materials;
  U32 materials_count;
} PIE_Builder;

typedef struct
{
  float height; // automatically adjust scale so model is equal height (meters) on Z axis
  bool disable_z0; // automatically move model to start at z = 0

  float scale;
  Quat rot;
  V3 move;

  S8 name; // If empty name will be taken from file name.
} BK_GLTF_Spec;

typedef struct
{
  // BK_GLTF_Mesh Groups all vertices and indicecs per common material in .gltf file.
  U32 material_index;

  U32 verts_count;
  BK_Buffer indices;
  BK_Buffer positions;
  BK_Buffer normals;
  BK_Buffer texcoords;
  BK_Buffer joint_indices;
  BK_Buffer weights;
} BK_GLTF_Mesh;

typedef struct
{
  BK_GLTF_Mesh *meshes;
  U32 meshes_count;

  S8 name;
  bool has_skeleton;
  U32 skeleton_index;

  bool is_skinned;
  U32 joints_count;

  BK_GLTF_Spec spec;
  Mat4 spec_transform;
} BK_GLTF_Model;

typedef struct
{
  bc7enc_compress_block_params params;
  SDL_Surface *bc7_block_surf;
  PIE_TexFormat format;
} BAKER_TexState;

typedef struct
{
  Arena *tmp;
  Arena *cgltf_arena;

  PIE_Builder pie_builder;

  BAKER_TexState tex;
} BAKER_State;
static BAKER_State BAKER;
