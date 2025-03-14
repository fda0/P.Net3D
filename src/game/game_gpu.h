typedef struct
{
  SDL_GPUBuffer *vert_buf;
  SDL_GPUBuffer *ind_buf;
  SDL_GPUBuffer *inst_buf;
  U32 ind_count;
} GPU_ModelBuffers;

typedef struct
{
  SDL_GPUDevice *device;

  U32 draw_width, draw_height;
  SDL_GPUTexture *tex_depth;
  SDL_GPUTexture *tex_msaa;
  SDL_GPUTexture *tex_resolve;

  SDL_GPUTexture *shadow_tex;
  SDL_GPUSampler *shadow_sampler;

  // pipeline, sample settings
  SDL_GPUGraphicsPipeline *rigid_pipeline;
  SDL_GPUGraphicsPipeline *skinned_pipeline;
  SDL_GPUGraphicsPipeline *wall_pipeline;
  SDL_GPUSampleCount sample_count;

  // resources
  GPU_ModelBuffers rigids[RdrRigid_COUNT];
  GPU_ModelBuffers skinneds[RdrSkinned_COUNT];
  SDL_GPUBuffer *skinned_pose_bufs[RdrSkinned_COUNT];

  SDL_GPUBuffer *wall_inst_buf;
  SDL_GPUSampler *wall_sampler;
  SDL_GPUBuffer *wall_vert_buffers[TEX_COUNT];
  SDL_GPUTexture *wall_texs[TEX_COUNT];

  SDL_PropertiesID clear_depth_props;
  SDL_PropertiesID clear_color_props;
} GPU_State;
