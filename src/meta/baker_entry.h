typedef struct
{
  Printer file;
  bool finalized;

  Printer rigid_vertices;
  Printer skinned_vertices;
  Printer indices;
  BREAD_Model models[MODEL_COUNT];

  Printer skeletons;
  U32 skeletons_count;

  Printer materials;
  U32 materials_count;

  // data for geometry that's currently under construction
  MODEL_Type selected_model;
  BREAD_Geometry *geos;
  U32 geos_count;
  U32 geos_index;
} BREAD_Builder;

typedef struct
{
  float scale;
  Quat rot;
  V3 move;
} BK_GLTF_ModelConfig;

typedef struct
{
  BK_GLTF_ModelConfig config;

  BK_Buffer indices;
  BK_Buffer positions;
  BK_Buffer normals;
  BK_Buffer texcoords;
  BK_Buffer joint_indices;
  BK_Buffer weights;
  BK_Buffer colors;

  S8 name; // @todo remove
  MODEL_Type type;
  bool is_skinned;
  U32 verts_count;
  U32 joints_count;

  bool has_skeleton;
  U32 skeleton_index;
} BK_GLTF_ModelData;

typedef struct
{
  bc7enc_compress_block_params params;
  SDL_Surface *bc7_block_surf;
} BAKER_TexState;

typedef struct
{
  Arena *tmp;
  Arena *cgltf_arena;

  BREAD_Builder bb;

  BAKER_TexState tex;
} BAKER_State;
static BAKER_State BAKER;
