typedef struct
{
  SDL_GPUDevice *device;
  GPU_MemoryState mem;

  SDL_GPUTexture *tex_depth;
  SDL_GPUTexture *tex_msaa;
  SDL_GPUTexture *tex_resolve;

  SDL_GPUTexture *shadow_tex;
  SDL_GPUSampler *shadow_sampler;

  SDL_GPUTexture *dummy_shadow_tex; // bound in shadowmap prepass; @todo delete and write special-purpose barebones fragment shader without textures for prepass
  SDL_GPUBuffer *dummy_instance_buffer;
  SDL_GPUBuffer *dummy_pose_buffer;

  // pipeline, sample settings
  SDL_GPUSampleCount sample_count;
  SDL_GPUGraphicsPipeline *world_pipelines[2]; // 0: 4xMSAA; 1: no AA (for shadow mapping)
  SDL_GPUGraphicsPipeline *ui_pipeline;

  U64 bound_uniform_hash;
  WORLD_Uniform world_uniform;

  SDL_GPUSampler *mesh_tex_sampler;

  struct
  {
    struct
    {
      SDL_GPUTexture *atlas_texture;
      SDL_GPUSampler *atlas_sampler;
      SDL_GPUBuffer *indices;
      SDL_GPUBuffer *shape_buffer;
      SDL_GPUBuffer *clip_buffer;
    } gpu;

    I32 indices[1024 * 8];
    U32 indices_count;

    UI_Shape shapes[1024 * 2];
    U32 shapes_count;

    UI_Clip clips[1024];
    U32 clips_count;
    U32 clip_stack[256];
    U32 clip_stack_index;
  } ui;

  // sdl properties
  SDL_PropertiesID clear_depth_props;
  SDL_PropertiesID clear_color_props;
} GPU_State;

static SDL_GPUBuffer *GPU_CreateBuffer(SDL_GPUBufferUsageFlags usage, U32 size, const char *name);
static void GPU_TransferBuffer(SDL_GPUBuffer *gpu_buffer, void *data, U32 data_size);
