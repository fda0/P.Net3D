#if 0
typedef struct
{
  SDL_GPUBuffer *vert_buf;
  SDL_GPUBuffer *ind_buf;
  SDL_GPUBuffer *inst_buf;
  U32 ind_count;
} GPU_ModelBuffers;
#endif

typedef struct
{
  SDL_GPUGraphicsPipeline *rigid;
  SDL_GPUGraphicsPipeline *skinned;
  SDL_GPUGraphicsPipeline *wall;
} GPU_WorldPipelines;

typedef struct
{
  SDL_GPUDevice *device;

  SDL_GPUTexture *tex_depth;
  SDL_GPUTexture *tex_msaa;
  SDL_GPUTexture *tex_resolve;

  SDL_GPUTexture *shadow_tex;
  SDL_GPUSampler *shadow_sampler;

  // pipeline, sample settings
  SDL_GPUSampleCount sample_count;
  GPU_WorldPipelines world_pipelines[2]; // 0: 4xMSAA; 1: no AA (for shadow mapping)
  SDL_GPUGraphicsPipeline *ui_pipeline;

  // model, mesh, ui resources
  struct
  {
    MDL_Batch batches[MDL_COUNT];
    Mat4 poses[128 * MDL_MAX_INSTANCES * MDL_COUNT];
    U32 poses_count;

    SDL_GPUBuffer *gpu_pose_buffer;
  } model;

  struct
  {
    MSH_Batch batches[TEX_COUNT];
    SDL_GPUSampler *gpu_sampler;
  } mesh;

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

    UI_GpuShape shapes[1024 * 2];
    U32 shapes_count;

    UI_GpuClip clips[1024];
    U32 clips_count;
  } ui;

  // sdl properties
  SDL_PropertiesID clear_depth_props;
  SDL_PropertiesID clear_color_props;
} GPU_State;
