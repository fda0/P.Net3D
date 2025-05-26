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

GPU_CLEAR_DEPTH_FLOAT :: 1.0;
GPU_CLEAR_COLOR_R :: 0.78;
GPU_CLEAR_COLOR_G :: 0.78;
GPU_CLEAR_COLOR_B :: 0.96;
GPU_CLEAR_COLOR_A :: 1.0;
GPU_SHADOW_MAP_DIM :: (2048*2);
GPU_ENABLE_BACKFACE_CULL :: 0;

GPU_Init :: ()
{
  // @todo
}

GPU_Iter :: ()
{
  // @todo
}

GPU_CreateBuffer :: (usage: SDL_GPUBufferUsageFlags, size: u32, name: string) -> SDL_GPUBuffer
{
  desc := SDL_GPUBufferCreateInfo.{usage = usage, size = size};
  result := SDL_CreateGPUBuffer(G.gpu.device, *desc);
  assert(!!result);
  if (name.count)
    SDL_SetGPUBufferName(G.gpu.device, result, temp_c_string(name));
  return result;
}

GPU_CreateDepthTexture :: (width: u32, height: u32, used_in_sampler: bool) -> *SDL_GPUTexture
{
  info := SDL_GPUTextureCreateInfo.{
    type = .SDL_GPU_TEXTURETYPE_2D,
    format = .D16_UNORM,
    width = width,
    height = height,
    layer_count_or_depth = 1,
    num_levels = 1,
    sample_count = G.gpu.sample_count,
    usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
    props = G.gpu.clear_depth_props,
  };

  if (used_in_sampler)
  {
    info.sample_count = .SDL_GPU_SAMPLECOUNT_1;
    info.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
  }

  result := SDL_CreateGPUTexture(G.gpu.device, *info);
  assert(!!result); // @todo report err
  return result;
}

GPU_CreateMSAATexture :: (width: u32, height: u32) -> *SDL_GPUTexture
{
  if (G.gpu.sample_count == .SDL_GPU_SAMPLECOUNT_1)
    return null;

  info := SDL_GPUTextureCreateInfo.{
    type = .SDL_GPU_TEXTURETYPE_2D,
    format = SDL_GetGPUSwapchainTextureFormat(G.gpu.device, G.window),
    width = width,
    height = height,
    layer_count_or_depth = 1,
    num_levels = 1,
    sample_count = G.gpu.sample_count,
    usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
    props = G.gpu.clear_color_props,
  };

  result := SDL_CreateGPUTexture(G.gpu.device, *info);
  assert(!!result); // @todo report err
  return result;
}

GPU_CreateResolveTexture :: (width: u32, height: u32) -> *SDL_GPUTexture
{
  if (G.gpu.sample_count == .SDL_GPU_SAMPLECOUNT_1)
    return null;

  info := SDL_GPUTextureCreateInfo.{
    type = .SDL_GPU_TEXTURETYPE_2D,
    format = SDL_GetGPUSwapchainTextureFormat(G.gpu.device, G.window),
    width = width,
    height = height,
    layer_count_or_depth = 1,
    num_levels = 1,
    sample_count = .SDL_GPU_SAMPLECOUNT_1,
    usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
    props = G.gpu.clear_color_props,
  };

  result := SDL_CreateGPUTexture(G.gpu.device, *info);
  assert(!!result); // @todo report err
  return result;
}
