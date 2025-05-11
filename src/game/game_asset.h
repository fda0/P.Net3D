typedef struct
{
  U32 vertices_start_index;
  U32 indices_start_index;
  U32 indices_count;
} ASSET_Geometry;

typedef struct
{
  U64 last_touched_frame;
  U32 error : 1;
  U32 loaded : 1;
  float loaded_t;
  union
  {
    struct
    {
      SDL_GPUTexture *handle;
      float shininess;
    } Tex;

    struct
    {
      bool is_skinned;
      U32 skeleton_index;
      ASSET_Geometry *geos;
      U32 geos_count;
    } Model;

    AN_Skeleton Skel;
  };
} Asset;

typedef struct
{
  Arena *arena;
  S8 file;
  BREAD_Header *header;
  BREAD_Links *links;

  BREAD_Model *models;
  U32 models_count;

  BREAD_Material *materials;
  U32 materials_count;
} AST_BreadFile;

typedef struct
{
  AST_BreadFile bread;

  // Texture
  SDL_GPUTexture *tex_fallback;
  Asset tex_assets[TEX_COUNT];
  // Texture thread
  bool tex_load_needed;
  SDL_Semaphore *tex_sem;

  // Skeleton
  Asset skeletons[MODEL_COUNT];
  U32 skeletons_count;

  // Geometry
  SDL_GPUBuffer *rigid_vertices;
  SDL_GPUBuffer *skinned_vertices;
  SDL_GPUBuffer *indices;
  Asset geo_assets[MODEL_COUNT];

  U64 serialize_last_check_timestamp;
  U64 serialize_hash;
} AST_State;

static void BREAD_LoadFile(const char *bread_file_path);
