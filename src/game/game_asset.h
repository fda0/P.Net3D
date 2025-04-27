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
      SDL_GPUBuffer *vertices;
      SDL_GPUBuffer *indices;
    } Geo;
  };
} Asset;

typedef struct
{
  // textures
  SDL_GPUTexture *tex_fallback;
  Asset tex_assets[TEX_COUNT];
  // textures thread
  bool tex_load_needed;
  SDL_Semaphore *tex_sem;

  U64 serialize_last_check_timestamp;
  U64 serialize_hash;
} AST_State;
