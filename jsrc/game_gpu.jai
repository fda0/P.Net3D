GPU_State :: struct
{
  device: *SDL_GPUDevice;

  tex_depth: *SDL_GPUTexture;
  tex_msaa: *SDL_GPUTexture;
  tex_resolve: *SDL_GPUTexture;

  shadow_tex: *SDL_GPUTexture;
  shadow_sampler: *SDL_GPUSampler;

  dummy_shadow_tex: *SDL_GPUTexture; // bound in shadowmap prepass; @todo delete?
  dummy_instance_buffer: *SDL_GPUBuffer;

  // pipeline, sample settings
  sample_count: SDL_GPUSampleCount;
  world_pipelines: [2] *SDL_GPUGraphicsPipeline; // 0: 4xMSAA; 1: no AA (for shadow mapping)
  ui_pipeline: *SDL_GPUGraphicsPipeline;

  bound_uniform_hash: u64;
  world_uniform: WORLD_Uniform;

  mesh_tex_sampler: *SDL_GPUSampler;

  UI_GPU :: struct
  {
    gpu_atlas_texture: *SDL_GPUTexture;
    gpu_atlas_sampler: *SDL_GPUSampler;
    gpu_indices: *SDL_GPUBuffer;
    gpu_shape_buffer: *SDL_GPUBuffer;
    gpu_clip_buffer: *SDL_GPUBuffer;

    indices: [1024 * 8] s32;
    indices_count: s32;

    shapes: [1024 * 2] UI_Shape;
    shapes_count: s32;

    clips: [1024] UI_Clip;
    clips_count: s32;
    clip_stack: [256] s32;
    clip_stack_index: s32;
  };
  ui: UI_GPU;

  // sdl properties
  clear_depth_props: SDL_PropertiesID;
  clear_color_props: SDL_PropertiesID;
}

GPU_Init :: ()
{
  // @todo
}
