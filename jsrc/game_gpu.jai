GPU_USE_MSAA :: true;
GPU_SHADOW_MAP_DIM :: 2048*2;
GPU_ENABLE_BACKFACE_CULL :: 0;
GPU_CLEAR_DEPTH_FLOAT :: 1.0;
GPU_CLEAR_COLOR_R :: 0.78;
GPU_CLEAR_COLOR_G :: 0.78;
GPU_CLEAR_COLOR_B :: 0.96;
GPU_CLEAR_COLOR_A :: 1.0;

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
  // preapre props
  {
    G.gpu.clear_depth_props = SDL_CreateProperties();
    SDL_SetFloatProperty(G.gpu.clear_depth_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_DEPTH_FLOAT,
                         GPU_CLEAR_DEPTH_FLOAT);

    G.gpu.clear_color_props = SDL_CreateProperties();
    SDL_SetFloatProperty(G.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_R_FLOAT,
                         GPU_CLEAR_COLOR_R);
    SDL_SetFloatProperty(G.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_G_FLOAT,
                         GPU_CLEAR_COLOR_G);
    SDL_SetFloatProperty(G.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_B_FLOAT,
                         GPU_CLEAR_COLOR_B);
    SDL_SetFloatProperty(G.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT,
                         GPU_CLEAR_COLOR_A);
  }

  // check for msaa 4 support
  G.gpu.sample_count = .SDL_GPU_SAMPLECOUNT_1;
  if (GPU_USE_MSAA)
  {
    tex_format := SDL_GetGPUSwapchainTextureFormat(G.gpu.device, G.window);
    supports_msaa := SDL_GPUTextureSupportsSampleCount(G.gpu.device, tex_format, .SDL_GPU_SAMPLECOUNT_4);
    if (supports_msaa)
      G.gpu.sample_count = .SDL_GPU_SAMPLECOUNT_4;
  }

  color_desc := SDL_GPUColorTargetDescription.{
    format = SDL_GetGPUSwapchainTextureFormat(G.gpu.device, G.window),
  };

  // Texture sampler
  {
    sampler_info := SDL_GPUSamplerCreateInfo.{
      min_filter = .SDL_GPU_FILTER_LINEAR,
      mag_filter = .SDL_GPU_FILTER_LINEAR,
      mipmap_mode = .SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
      address_mode_u = .SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
      address_mode_v = .SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
      address_mode_w = .SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
      min_lod = 0,
      max_lod = 100,
    };
    G.gpu.mesh_tex_sampler = SDL_CreateGPUSampler(G.gpu.device, *sampler_info);
  }

  // WORLD pipeline
  {
    vertex_shader: *SDL_GPUShader;
    {
      create_info := SDL_GPUShaderCreateInfo.{
        stage = .SDL_GPU_SHADERSTAGE_VERTEX,
        num_samplers = 0,
        num_storage_buffers = 2,
        num_storage_textures = 0,
        num_uniform_buffers = 1,

        format = SDL_GPU_SHADERFORMAT_DXIL,
        code = *g_WORLD_DxShaderVS[0],
        code_size = size_of(type_of(g_WORLD_DxShaderVS)),
        entrypoint = "World_DxShaderSkinnedVS",
      };
      vertex_shader = SDL_CreateGPUShader(G.gpu.device, *create_info);
    }

    fragment_shader: *SDL_GPUShader;
    {
      create_info := SDL_GPUShaderCreateInfo.{
        stage = .SDL_GPU_SHADERSTAGE_FRAGMENT,
        num_samplers = 2,
        num_storage_buffers = 0,
        num_storage_textures = 0,
        num_uniform_buffers = 1,

        format = SDL_GPU_SHADERFORMAT_DXIL,
        code = *g_WORLD_DxShaderPS[0],
        code_size = size_of(type_of(g_WORLD_DxShaderPS)),
        entrypoint = "World_DxShaderSkinnedPS",
      };
      fragment_shader = SDL_CreateGPUShader(G.gpu.device, *create_info);
    }

    pipeline := SDL_GPUGraphicsPipelineCreateInfo.{
      vertex_shader = vertex_shader,
      fragment_shader = fragment_shader,
      multisample_state = SDL_GPUMultisampleState.{
        sample_count = G.gpu.sample_count,
      },
      depth_stencil_state = .{
        enable_depth_test = true,
        enable_depth_write = true,
        compare_op = .SDL_GPU_COMPAREOP_LESS,
      },
      primitive_type = .SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      rasterizer_state = .{
        front_face = .SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        cull_mode = ifx GPU_ENABLE_BACKFACE_CULL then .SDL_GPU_CULLMODE_BACK else .SDL_GPU_CULLMODE_NONE,
        enable_depth_clip = true,
      },
      target_info = .{
        num_color_targets = 1,
        color_target_descriptions = *color_desc,
        depth_stencil_format = .SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        has_depth_stencil_target = true,
      },
      props = 0
    };

    vbd := SDL_GPUVertexBufferDescription.[
      .{slot = 0,
        pitch = size_of(type_of(WORLD_Vertex)),
        input_rate = .SDL_GPU_VERTEXINPUTRATE_VERTEX,
        instance_step_rate = 0,
      },
    ];
    pipeline.vertex_input_state.num_vertex_buffers = vbd.count;
    pipeline.vertex_input_state.vertex_buffer_descriptions = *vbd[0];

    va := SDL_GPUVertexAttribute.[
      .{format = .SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        location = 0,
        offset = #run offset_of(WORLD_Vertex, "p"), // 0
      },
      .{format = .SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        location = 1,
        offset = #run offset_of(WORLD_Vertex, "normal"),
      },
      .{format = .SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        location = 2,
        offset = #run offset_of(WORLD_Vertex, "uv"),
      },
      .{format = .SDL_GPU_VERTEXELEMENTFORMAT_UINT,
        location = 3,
        offset = #run offset_of(WORLD_Vertex, "joints_packed4"),
      },
      .{format = .SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        location = 4,
        offset = #run offset_of(WORLD_Vertex, "joint_weights"),
      },
    ];
    pipeline.vertex_input_state.num_vertex_attributes = va.count;
    pipeline.vertex_input_state.vertex_attributes = *va[0];

    // Create main pipeline
    G.gpu.world_pipelines[0] = SDL_CreateGPUGraphicsPipeline(G.gpu.device, *pipeline);
    assert(!!G.gpu.world_pipelines[0]);

    // Modify pipeline settings for shadow mapping depth pass
    {
      pipeline.multisample_state.sample_count = .SDL_GPU_SAMPLECOUNT_1;
      pipeline.rasterizer_state.cull_mode = .SDL_GPU_CULLMODE_NONE;
    }

    // Create shadow pass pipeline
    G.gpu.world_pipelines[1] = SDL_CreateGPUGraphicsPipeline(G.gpu.device, *pipeline);
    assert(!!G.gpu.world_pipelines[1]);

    SDL_ReleaseGPUShader(G.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(G.gpu.device, fragment_shader);
  }

  G.gpu.dummy_instance_buffer = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                                                 xx Kilobyte(1), "Dummy instance storage buffer");

  // Shadow map
  {
    G.gpu.shadow_tex = GPU_CreateDepthTexture(GPU_SHADOW_MAP_DIM, GPU_SHADOW_MAP_DIM, true);
    G.gpu.dummy_shadow_tex = GPU_CreateDepthTexture(16, 16, true);

    sampler_info := SDL_GPUSamplerCreateInfo.{
      min_filter = .SDL_GPU_FILTER_LINEAR,
      mag_filter = .SDL_GPU_FILTER_LINEAR,
      mipmap_mode = .SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
      address_mode_u = .SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      address_mode_v = .SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      address_mode_w = .SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      min_lod = 0,
      max_lod = 100,
    };
    G.gpu.shadow_sampler = SDL_CreateGPUSampler(G.gpu.device, *sampler_info);
  }

  // UI sampler
  {
    sampler_info := SDL_GPUSamplerCreateInfo.{
      min_filter = .SDL_GPU_FILTER_LINEAR,
      mag_filter = .SDL_GPU_FILTER_LINEAR,
    };
    G.gpu.ui.gpu_atlas_sampler = SDL_CreateGPUSampler(G.gpu.device, *sampler_info);
  }

  // UI buffers
  {
    G.gpu.ui.gpu_indices =
      GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX, size_of(type_of(G.gpu.ui.indices)), "UI Index Buffer");
    G.gpu.ui.gpu_shape_buffer =
      GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, size_of(type_of(G.gpu.ui.shapes)), "UI Shapes Storage Buffer");
    G.gpu.ui.gpu_clip_buffer =
      GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, size_of(type_of(G.gpu.ui.clips)), "UI Clips Storage Buffer");
  }

  // UI pipeline
  {
    vertex_shader: *SDL_GPUShader;
    {
      create_info := SDL_GPUShaderCreateInfo.{
        stage = .SDL_GPU_SHADERSTAGE_VERTEX,
        num_samplers = 0,
        num_storage_buffers = 2,
        num_storage_textures = 0,
        num_uniform_buffers = 1,

        format = SDL_GPU_SHADERFORMAT_DXIL,
        code = *g_UI_DxShaderVS[0],
        code_size = size_of(type_of(g_UI_DxShaderVS)),
        entrypoint = "UI_DxShaderVS",
      };
      vertex_shader = SDL_CreateGPUShader(G.gpu.device, *create_info);
    }

    fragment_shader: *SDL_GPUShader;
    {
      create_info := SDL_GPUShaderCreateInfo.{
        stage = .SDL_GPU_SHADERSTAGE_FRAGMENT,
        num_samplers = 1,
        num_storage_buffers = 0,
        num_storage_textures = 0,
        num_uniform_buffers = 0,

        format = SDL_GPU_SHADERFORMAT_DXIL,
        code = *g_UI_DxShaderPS[0],
        code_size = size_of(type_of(g_UI_DxShaderPS)),
        entrypoint = "UI_DxShaderPS",
      };
      fragment_shader = SDL_CreateGPUShader(G.gpu.device, *create_info);
    }

    ui_color_desc := SDL_GPUColorTargetDescription.{
      format = color_desc.format,
      blend_state = .{
        src_color_blendfactor = .SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        dst_color_blendfactor = .SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        color_blend_op = .SDL_GPU_BLENDOP_ADD,
        // use ONE, ONE instead?
        src_alpha_blendfactor = .SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        dst_alpha_blendfactor = .SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        alpha_blend_op = .SDL_GPU_BLENDOP_ADD,
        enable_blend = true,
      },
    };

    pipeline := SDL_GPUGraphicsPipelineCreateInfo.{
      vertex_shader = vertex_shader,
      fragment_shader = fragment_shader,
      primitive_type = .SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      target_info = .{
        num_color_targets = 1,
        color_target_descriptions = *ui_color_desc,
      },
    };
    G.gpu.ui_pipeline = SDL_CreateGPUGraphicsPipeline(G.gpu.device, *pipeline);

    SDL_ReleaseGPUShader(G.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(G.gpu.device, fragment_shader);
  }

  GPU_ProcessWindowResize(true);
}

GPU_Iter :: ()
{
  // @todo
}

GPU_ProcessWindowResize :: (init: bool)
{
  if (G.window_resized || init)
  {
    SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.tex_depth);
    SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.tex_msaa);
    SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.tex_resolve);

    window_width := cast(u32) G.window_dim.x;
    window_height := cast(u32) G.window_dim.y;
    G.gpu.tex_depth = GPU_CreateDepthTexture(window_width, window_height, false);
    G.gpu.tex_msaa = GPU_CreateMSAATexture(window_width, window_height);
    G.gpu.tex_resolve = GPU_CreateResolveTexture(window_width, window_height);
  }
}

GPU_CreateBuffer :: (usage: SDL_GPUBufferUsageFlags, size: u32, name: string) -> *SDL_GPUBuffer
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

GPU_TransferBuffer :: (gpu_buffer: *SDL_GPUBuffer, data: *void, data_size: u32)
{
  // create transfer buffer
  trans_desc := SDL_GPUTransferBufferCreateInfo.{
    usage = .SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    size = data_size
  };
  buf_transfer := SDL_CreateGPUTransferBuffer(G.gpu.device, *trans_desc);
  assert(!!buf_transfer); // @todo report err

  // CPU memory -> GPU memory
  {
    map : *void = SDL_MapGPUTransferBuffer(G.gpu.device, buf_transfer, false);
    memcpy(map, data, data_size);
    SDL_UnmapGPUTransferBuffer(G.gpu.device, buf_transfer);
  }

  // GPU memory -> GPU buffers
  {
    cmd := SDL_AcquireGPUCommandBuffer(G.gpu.device);
    copy_pass := SDL_BeginGPUCopyPass(cmd);

    buf_location := SDL_GPUTransferBufferLocation.{
      transfer_buffer = buf_transfer,
      offset = 0,
    };
    dst_region := SDL_GPUBufferRegion.{
      buffer = gpu_buffer,
      offset = 0,
      size = data_size,
    };
    SDL_UploadToGPUBuffer(copy_pass, *buf_location, *dst_region, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);
  }

  SDL_ReleaseGPUTransferBuffer(G.gpu.device, buf_transfer);
}

GPU_UpdateWorldUniform :: (cmd: *SDL_GPUCommandBuffer, uniform: WORLD_Uniform)
{
  uniform_hash := Hash64T(0, uniform);
  if (G.gpu.bound_uniform_hash != uniform_hash)
  {
    G.gpu.bound_uniform_hash = uniform_hash;
    SDL_PushGPUVertexUniformData(cmd, 0, *uniform, size_of(type_of(uniform)));
    SDL_PushGPUFragmentUniformData(cmd, 0, *uniform, size_of(type_of(uniform)));
  }
}

GPU_UpdateUIUniform :: (cmd: *SDL_GPUCommandBuffer, uniform: UI_Uniform)
{
  uniform_hash := Hash64T(0, uniform);
  if (G.gpu.bound_uniform_hash != uniform_hash)
  {
    G.gpu.bound_uniform_hash = uniform_hash;
    SDL_PushGPUVertexUniformData(cmd, 0, *uniform, size_of(type_of(uniform)));
    // SDL_PushGPUFragmentUniformData(cmd, 0, *uniform, size_of(type_of(uniform)));
  }
}

GPU_Deinit :: ()
{
  for G.gpu.world_pipelines
  {
    SDL_ReleaseGPUGraphicsPipeline(G.gpu.device, it);
  }
  SDL_ReleaseGPUGraphicsPipeline(G.gpu.device, G.gpu.ui_pipeline);

  SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.tex_depth);
  SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.tex_msaa);
  SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.tex_resolve);

  SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.shadow_tex);
  SDL_ReleaseGPUSampler(G.gpu.device, G.gpu.shadow_sampler);

  SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.dummy_shadow_tex);
  SDL_ReleaseGPUBuffer(G.gpu.device, G.gpu.dummy_instance_buffer);

  // Mesh
  SDL_ReleaseGPUSampler(G.gpu.device, G.gpu.mesh_tex_sampler);

  // UI
  SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.ui.gpu_atlas_texture);
  SDL_ReleaseGPUSampler(G.gpu.device, G.gpu.ui.gpu_atlas_sampler);
  SDL_ReleaseGPUBuffer(G.gpu.device, G.gpu.ui.gpu_indices);
  SDL_ReleaseGPUBuffer(G.gpu.device, G.gpu.ui.gpu_shape_buffer);
  SDL_ReleaseGPUBuffer(G.gpu.device, G.gpu.ui.gpu_clip_buffer);
}
