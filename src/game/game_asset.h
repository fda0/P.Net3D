typedef struct
{
  SDL_GPUTexture *texture;
  U64 last_touched_frame;
  float shininess;
  float loaded_t;
  U32 error : 1;
  U32 loaded : 1;
} Asset;

typedef struct
{
  // textures
  SDL_GPUTexture *tex_fallback;
  Asset tex_assets[TEX_COUNT];
  // textures thread
  bool tex_load_needed;
  SDL_Semaphore *tex_sem;
} AST_State;
