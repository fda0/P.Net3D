#define GPU_CLEAR_DEPTH_FLOAT 1.0f
#define GPU_CLEAR_COLOR_R 0.78f
#define GPU_CLEAR_COLOR_G 0.78f
#define GPU_CLEAR_COLOR_B 0.96f
#define GPU_CLEAR_COLOR_A 1.0f
#define GPU_JOINT_TRANSFORMS_MAX_SIZE (sizeof(Mat4)*62)
#define GPU_SHADOW_MAP_DIM 2048

static SDL_GPUBuffer *GPU_CreateBuffer(SDL_GPUBufferUsageFlags usage, U32 size, const char *name)
{
  SDL_GPUBufferCreateInfo desc = {.usage = usage, .size = size};
  SDL_GPUBuffer *result = SDL_CreateGPUBuffer(APP.gpu.device, &desc);
  Assert(result);
  if (name)
    SDL_SetGPUBufferName(APP.gpu.device, result, name);
  return result;
}

static SDL_GPUTexture *GPU_CreateDepthTexture(U32 width, U32 height, bool used_in_sampler)
{
  SDL_GPUTextureCreateInfo info = {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    .width = width,
    .height = height,
    .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = APP.gpu.sample_count,
    .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
    .props = APP.gpu.clear_depth_props,
  };

  if (used_in_sampler)
  {
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
  }

  SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &info);
  Assert(result); // @todo report err
  return result;
}

static SDL_GPUTexture *GPU_CreateMSAATexture(U32 width, U32 height)
{
  if (APP.gpu.sample_count == SDL_GPU_SAMPLECOUNT_1)
    return 0;

  SDL_GPUTextureCreateInfo createinfo = {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
    .width = width,
    .height = height,
    .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = APP.gpu.sample_count,
    .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
    .props = APP.gpu.clear_color_props,
  };

  SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);
  Assert(result); // @todo report err
  return result;
}

static SDL_GPUTexture *GPU_CreateResolveTexture(U32 width, U32 height)
{
  if (APP.gpu.sample_count == SDL_GPU_SAMPLECOUNT_1)
    return 0;

  SDL_GPUTextureCreateInfo createinfo = {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
    .width = width,
    .height = height,
    .layer_count_or_depth = 1,
    .num_levels = 1,
    .sample_count = SDL_GPU_SAMPLECOUNT_1,
    .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .props = APP.gpu.clear_color_props,
  };

  SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);
  Assert(result); // @todo report err
  return result;
}

static void GPU_ProcessWindowResize(bool init)
{
  if (APP.window_resized || init)
  {
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_depth);
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_msaa);
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_resolve);

    APP.gpu.tex_depth = GPU_CreateDepthTexture(APP.window_width, APP.window_height, false);
    APP.gpu.tex_msaa = GPU_CreateMSAATexture(APP.window_width, APP.window_height);
    APP.gpu.tex_resolve = GPU_CreateResolveTexture(APP.window_width, APP.window_height);
  }
}

static void GPU_TransferBuffer(SDL_GPUBuffer *gpu_buffer, void *data, U32 data_size)
{
  // create transfer buffer
  SDL_GPUTransferBufferCreateInfo trans_desc =
  {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = data_size
  };
  SDL_GPUTransferBuffer *buf_transfer = SDL_CreateGPUTransferBuffer(APP.gpu.device, &trans_desc);
  Assert(buf_transfer); // @todo report err

  // CPU memory -> GPU memory
  {
    void *map = SDL_MapGPUTransferBuffer(APP.gpu.device, buf_transfer, false);
    memcpy(map, data, data_size);
    SDL_UnmapGPUTransferBuffer(APP.gpu.device, buf_transfer);
  }

  // GPU memory -> GPU buffers
  {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation buf_location = {
      .transfer_buffer = buf_transfer,
      .offset = 0,
    };
    SDL_GPUBufferRegion dst_region = {
      .buffer = gpu_buffer,
      .offset = 0,
      .size = data_size,
    };
    SDL_UploadToGPUBuffer(copy_pass, &buf_location, &dst_region, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);
  }

  SDL_ReleaseGPUTransferBuffer(APP.gpu.device, buf_transfer);
}

static void GPU_TransferTexture(SDL_GPUTexture *gpu_tex,
                                U32 layer, U32 w, U32 h,
                                void *data, U32 data_size)
{
  // Create transfer buffer
  SDL_GPUTransferBufferCreateInfo trans_desc =
  {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = data_size
  };
  SDL_GPUTransferBuffer *trans_buf = SDL_CreateGPUTransferBuffer(APP.gpu.device, &trans_desc);
  Assert(trans_buf); // @todo report err

  // CPU memory -> GPU memory
  {
    void *map = SDL_MapGPUTransferBuffer(APP.gpu.device, trans_buf, false);
    memcpy(map, data, data_size);
    SDL_UnmapGPUTransferBuffer(APP.gpu.device, trans_buf);
  }

  // GPU memory -> GPU buffers
  {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo trans_info =
    {
      .transfer_buffer = trans_buf,
      .offset = 0,
    };
    SDL_GPUTextureRegion dst_region =
    {
      .texture = gpu_tex,
      .layer = layer,
      .w = w,
      .h = h,
      .d = 1,
    };
    SDL_UploadToGPUTexture(copy_pass, &trans_info, &dst_region, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);
  }

  SDL_ReleaseGPUTransferBuffer(APP.gpu.device, trans_buf);
}


static SDL_GPUGraphicsPipelineCreateInfo GPU_DefaultPipeline(SDL_GPUColorTargetDescription *color,
                                                             SDL_GPUShader *vertex,
                                                             SDL_GPUShader *fragment)
{
  SDL_GPUGraphicsPipelineCreateInfo pipeline =
  {
    .vertex_shader = vertex,
    .fragment_shader = fragment,
    .multisample_state =
    {
      .sample_count = APP.gpu.sample_count,
    },
    .depth_stencil_state =
    {
      .enable_depth_test = true,
      .enable_depth_write = true,
      .compare_op = SDL_GPU_COMPAREOP_LESS,
    },
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .rasterizer_state =
    {
      .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
      .cull_mode = SDL_GPU_CULLMODE_BACK,
      .enable_depth_clip = true,
    },
    .target_info =
    {
      .num_color_targets = 1,
      .color_target_descriptions = color,
      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .has_depth_stencil_target = true,
    },
    .props = 0
  };
  return pipeline;
}

static void GPU_ModifyPipelineForShadowMapping(SDL_GPUGraphicsPipelineCreateInfo *pipeline)
{
  pipeline->multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  pipeline->rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
}

static void GPU_Init()
{
  GPU_PostFrameClear();

  // preapre props
  {
    APP.gpu.clear_depth_props = SDL_CreateProperties();
    SDL_SetFloatProperty(APP.gpu.clear_depth_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_DEPTH_FLOAT,
                         GPU_CLEAR_DEPTH_FLOAT);

    APP.gpu.clear_color_props = SDL_CreateProperties();
    SDL_SetFloatProperty(APP.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_R_FLOAT,
                         GPU_CLEAR_COLOR_R);
    SDL_SetFloatProperty(APP.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_G_FLOAT,
                         GPU_CLEAR_COLOR_G);
    SDL_SetFloatProperty(APP.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_B_FLOAT,
                         GPU_CLEAR_COLOR_B);
    SDL_SetFloatProperty(APP.gpu.clear_color_props,
                         SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT,
                         GPU_CLEAR_COLOR_A);
  }

  // check for msaa 4 support
  APP.gpu.sample_count = SDL_GPU_SAMPLECOUNT_1;
  if (GPU_USE_MSAA)
  {
    SDL_GPUTextureFormat tex_format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window);
    bool supports_msaa = SDL_GPUTextureSupportsSampleCount(APP.gpu.device, tex_format, SDL_GPU_SAMPLECOUNT_4);
    if (supports_msaa)
      APP.gpu.sample_count = SDL_GPU_SAMPLECOUNT_4;
  }

  SDL_GPUColorTargetDescription color_desc =
  {
    .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
  };

  // Rigid pipeline
  {
    SDL_GPUShader *vertex_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_buffers = 1,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_World_DxShaderRigidVS,
        .code_size = sizeof(g_World_DxShaderRigidVS),
        .entrypoint = "World_DxShaderRigidVS",
      };
      vertex_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUShader *fragment_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_World_DxShaderRigidPS,
        .code_size = sizeof(g_World_DxShaderRigidPS),
        .entrypoint = "World_DxShaderRigidPS",
      };
      fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline =
      GPU_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(WORLD_VertexRigid),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 3,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 0,
          .offset = 0,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = offsetof(WORLD_VertexRigid, p),
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 2,
          .offset = offsetof(WORLD_VertexRigid, color),
        },
      },
    };


    APP.gpu.world_pipelines[0].rigid = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.world_pipelines[0].rigid);

    GPU_ModifyPipelineForShadowMapping(&pipeline);
    APP.gpu.world_pipelines[1].rigid = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.world_pipelines[1].rigid);

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  // Skinned pipeline
  {
    SDL_GPUShader *vertex_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_buffers = 2,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_World_DxShaderSkinnedVS,
        .code_size = sizeof(g_World_DxShaderSkinnedVS),
        .entrypoint = "World_DxShaderSkinnedVS",
      };
      vertex_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUShader *fragment_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_World_DxShaderSkinnedPS,
        .code_size = sizeof(g_World_DxShaderSkinnedPS),
        .entrypoint = "World_DxShaderSkinnedPS",
      };
      fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline =
      GPU_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(WORLD_VertexSkinned),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 5,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 0,
          .offset = 0,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = offsetof(WORLD_VertexSkinned, p),
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 2,
          .offset = offsetof(WORLD_VertexSkinned, color),
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 3,
          .offset = offsetof(WORLD_VertexSkinned, joints_packed4),
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .location = 4,
          .offset = offsetof(WORLD_VertexSkinned, weights),
        },
      },
    };


    APP.gpu.world_pipelines[0].skinned = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.world_pipelines[0].skinned);

    GPU_ModifyPipelineForShadowMapping(&pipeline);
    APP.gpu.world_pipelines[1].skinned = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.world_pipelines[1].skinned);

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  // Mesh pipeline
  {
    // Texture sampler
    {
      SDL_GPUSamplerCreateInfo sampler_info =
      {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .min_lod = 0.f,
        .max_lod = 100.f,
      };
      APP.gpu.mesh_tex_sampler = SDL_CreateGPUSampler(APP.gpu.device, &sampler_info);
    }

    SDL_GPUShader *vertex_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_World_DxShaderMeshVS,
        .code_size = sizeof(g_World_DxShaderMeshVS),
        .entrypoint = "World_DxShaderMeshVS",
      };
      vertex_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUShader *fragment_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 2,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_World_DxShaderMeshPS,
        .code_size = sizeof(g_World_DxShaderMeshPS),
        .entrypoint = "World_DxShaderMeshPS",
      };
      fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline =
      GPU_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(WORLD_VertexMesh),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 3,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 0,
          .offset = 0,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = offsetof(WORLD_VertexMesh, p),
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .location = 2,
          .offset = offsetof(WORLD_VertexMesh, uv),
        },
      },
    };


    APP.gpu.world_pipelines[0].wall = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.world_pipelines[0].wall);

    GPU_ModifyPipelineForShadowMapping(&pipeline);
    APP.gpu.world_pipelines[1].wall = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.world_pipelines[1].wall);

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  // Shadow map
  {
    APP.gpu.shadow_tex = GPU_CreateDepthTexture(GPU_SHADOW_MAP_DIM, GPU_SHADOW_MAP_DIM, true);
    APP.gpu.dummy_shadow_tex = GPU_CreateDepthTexture(16, 16, true);

    SDL_GPUSamplerCreateInfo sampler_info =
    {
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .min_lod = 0.f,
      .max_lod = 100.f,
    };
    APP.gpu.shadow_sampler = SDL_CreateGPUSampler(APP.gpu.device, &sampler_info);
  }

  // UI sampler
  {
    SDL_GPUSamplerCreateInfo sampler_info =
    {
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
    };
    APP.gpu.ui.gpu.atlas_sampler = SDL_CreateGPUSampler(APP.gpu.device, &sampler_info);
  }

  // UI buffers
  {
    APP.gpu.ui.gpu.indices =
      GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX, sizeof(APP.gpu.ui.indices), "UI Index Buffer");
    APP.gpu.ui.gpu.shape_buffer =
      GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, sizeof(APP.gpu.ui.shapes), "UI Shapes Storage Buffer");
    APP.gpu.ui.gpu.clip_buffer =
      GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, sizeof(APP.gpu.ui.clips), "UI Clips Storage Buffer");
  }

  // UI pipeline
  {
    SDL_GPUShader *vertex_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_buffers = 2,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_UI_DxShaderVS,
        .code_size = sizeof(g_UI_DxShaderVS),
        .entrypoint = "UI_DxShaderVS",
      };
      vertex_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUShader *fragment_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
        .num_uniform_buffers = 0,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_UI_DxShaderPS,
        .code_size = sizeof(g_UI_DxShaderPS),
        .entrypoint = "UI_DxShaderPS",
      };
      fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUColorTargetDescription ui_color_desc =
    {
      .format = color_desc.format,
      .blend_state = (SDL_GPUColorTargetBlendState)
      {
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        // @todo use ONE, ONE instead?
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .enable_blend = true,
      },
    };

    SDL_GPUGraphicsPipelineCreateInfo pipeline =
    {
      .vertex_shader = vertex_shader,
      .fragment_shader = fragment_shader,
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .target_info =
      {
        .num_color_targets = 1,
        .color_target_descriptions = &ui_color_desc,
      },
    };
    APP.gpu.ui_pipeline = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  GPU_ProcessWindowResize(true);
}

static void GPU_Deinit()
{
  ForArray(i, APP.gpu.world_pipelines)
  {
    SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.world_pipelines[i].rigid);
    SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.world_pipelines[i].skinned);
    SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.world_pipelines[i].wall);
  }
  SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.ui_pipeline);

  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_depth);
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_msaa);
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_resolve);

  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.shadow_tex);
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.dummy_shadow_tex);
  SDL_ReleaseGPUSampler(APP.gpu.device, APP.gpu.shadow_sampler);

  GPU_MemoryDeinit();

  // Mesh
  SDL_ReleaseGPUSampler(APP.gpu.device, APP.gpu.mesh_tex_sampler);

  // UI
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.ui.gpu.atlas_texture);
  SDL_ReleaseGPUSampler(APP.gpu.device, APP.gpu.ui.gpu.atlas_sampler);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.ui.gpu.indices);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.ui.gpu.shape_buffer);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.ui.gpu.clip_buffer);
}

static void GPU_UpdateWorldUniform(SDL_GPUCommandBuffer *cmd, WORLD_Uniform uniform)
{
  U64 uniform_hash = U64_Hash(sizeof(uniform), &uniform, sizeof(uniform));
  if (APP.gpu.bound_uniform_hash != uniform_hash)
  {
    APP.gpu.bound_uniform_hash = uniform_hash;
    SDL_PushGPUVertexUniformData(cmd, 0, &uniform, sizeof(uniform));
    SDL_PushGPUFragmentUniformData(cmd, 0, &uniform, sizeof(uniform));
  }
}

static void GPU_UpdateUIUniform(SDL_GPUCommandBuffer *cmd, UI_Uniform uniform)
{
  U64 uniform_hash = U64_Hash(sizeof(uniform), &uniform, sizeof(uniform));
  if (APP.gpu.bound_uniform_hash != uniform_hash)
  {
    APP.gpu.bound_uniform_hash = uniform_hash;
    SDL_PushGPUVertexUniformData(cmd, 0, &uniform, sizeof(uniform));
    //SDL_PushGPUFragmentUniformData(cmd, 0, &uniform, sizeof(uniform));
  }
}

static void GPU_DrawWorld(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass, bool is_depth_prepass)
{
  U32 pipeline_index = is_depth_prepass ? 1 : 0;
  AssertBounds(pipeline_index, APP.gpu.world_pipelines);

  // Bind shadow texture sampler to fragment shader
  {
    SDL_GPUTextureSamplerBinding binding_sampl =
    {
      .texture = is_depth_prepass ? APP.gpu.dummy_shadow_tex : APP.gpu.shadow_tex,
      .sampler = APP.gpu.shadow_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &binding_sampl, 1);
  }

  // Meshes (walls)
  SDL_BindGPUGraphicsPipeline(pass, APP.gpu.world_pipelines[pipeline_index].wall);
  {
    ForU32(tex_index, TEX_COUNT)
    {
      GPU_MemoryBundle *gpu_bundle =
        GPU_MemoryTargetToBundle((GPU_MemoryTarget){.type = GPU_MemoryMeshVertices,
                                                    .tex = tex_index});
      if (!gpu_bundle->element_count)
        continue;

      // Bind vertex buffer
      SDL_BindGPUVertexBuffers(pass, 0, &(SDL_GPUBufferBinding){.buffer = gpu_bundle->buffer->handle}, 1);

      ASSET_Texture *tex = ASSET_GetTexture(tex_index);

      // Update uniform
      if (!is_depth_prepass)
      {
        APP.gpu.world_uniform.tex_loaded_t = tex->b.loaded_t;
        APP.gpu.world_uniform.tex_shininess = tex->shininess;
        GPU_UpdateWorldUniform(cmd, APP.gpu.world_uniform);
      }

      // Bind fragment color texture sampler
      SDL_GPUTextureSamplerBinding binding_sampl =
      {
        .texture = tex->handle,
        .sampler = APP.gpu.mesh_tex_sampler,
      };
      SDL_BindGPUFragmentSamplers(pass, 1, &binding_sampl, 1);

      SDL_DrawGPUPrimitives(pass, gpu_bundle->element_count, 1, 0, 0);
    }
  }

  //
  // Draw rigid models
  //
  SDL_BindGPUGraphicsPipeline(pass, APP.gpu.world_pipelines[pipeline_index].rigid);

  // bind rigid vertex buffer
  SDL_BindGPUVertexBuffers(pass, 0, &(SDL_GPUBufferBinding){.buffer = APP.ast.rigid_vertices}, 1);

  // bind index buffer for all models
  SDL_BindGPUIndexBuffer(pass, &(SDL_GPUBufferBinding){.buffer = APP.ast.indices}, SDL_GPU_INDEXELEMENTSIZE_16BIT);

  ForU32(model_type, MODEL_COUNT)
  {
    ASSET_Model *model = ASSET_GetModel(model_type);
    if (model->is_skinned)
      continue;

    GPU_MemoryBundle *gpu_bundle =
      GPU_MemoryTargetToBundle((GPU_MemoryTarget){.type = GPU_MemoryModelInstances, .model = model_type});

    if (!gpu_bundle->element_count)
      continue;

    // bind instance storage buffer
    SDL_GPUBuffer *storage_bufs[1] = { gpu_bundle->buffer->handle };
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage_bufs, ArrayCount(storage_bufs));

    GPU_UpdateWorldUniform(cmd, APP.gpu.world_uniform);

    ForU32(geo_index, model->geos_count)
    {
      ASSET_Geometry *geo = model->geos + geo_index;
      SDL_DrawGPUIndexedPrimitives(pass, geo->indices_count, gpu_bundle->element_count,
                                   geo->indices_start_index, geo->vertices_start_index, 0);
    }
  }

  //
  // Draw skinned models
  //
  SDL_BindGPUGraphicsPipeline(pass, APP.gpu.world_pipelines[pipeline_index].skinned);

  // Bind skinned vertex buffer
  SDL_BindGPUVertexBuffers(pass, 0, &(SDL_GPUBufferBinding){.buffer = APP.ast.skinned_vertices}, 1);

  // Get joint transform buffer
  GPU_MemoryBundle *joints = GPU_MemoryTargetToBundle((GPU_MemoryTarget){.type = GPU_MemoryJointTransforms});

  ForU32(model_type, MODEL_COUNT)
  {
    ASSET_Model *model = ASSET_GetModel(model_type);
    if (!model->is_skinned)
      continue;

    GPU_MemoryBundle *gpu_bundle =
      GPU_MemoryTargetToBundle((GPU_MemoryTarget){.type = GPU_MemoryModelInstances, .model = model_type});

    if (!gpu_bundle->element_count)
      continue;

    // bind instance storage buffer
    SDL_GPUBuffer *storage_bufs[2] =
    {
      gpu_bundle->buffer->handle,
      joints->buffer->handle
    };
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage_bufs, ArrayCount(storage_bufs));

    GPU_UpdateWorldUniform(cmd, APP.gpu.world_uniform);

    ForU32(geo_index, model->geos_count)
    {
      ASSET_Geometry *geo = model->geos + geo_index;
      SDL_DrawGPUIndexedPrimitives(pass, geo->indices_count, gpu_bundle->element_count,
                                   geo->indices_start_index, geo->vertices_start_index, 0);
    }
  }
}

static void GPU_Iterate()
{
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
  Assert(cmd); // @todo report err

  SDL_GPUTexture *swapchain_tex = 0;
  U32 draw_width, draw_height;
  bool res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd, APP.window, &swapchain_tex, &draw_width, &draw_height);
  Assert(res); // @todo report err

  if (!swapchain_tex)
  {
    // Swapchain is unavailable, cancel work
    SDL_CancelGPUCommandBuffer(cmd);
    return;
  }

  GPU_TransfersToBuffers(cmd);

  SDL_GPUDepthStencilTargetInfo depth_target =
  {
    .clear_depth = GPU_CLEAR_DEPTH_FLOAT,
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_DONT_CARE,
    .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
    .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
    .cycle = true,
  };

  APP.gpu.world_uniform = (WORLD_Uniform)
  {
    .camera_transform = APP.sun_camera_transform,
    .shadow_transform = APP.sun_camera_transform,
    .camera_position = APP.camera_p,
    .background_color = (V3){GPU_CLEAR_COLOR_R, GPU_CLEAR_COLOR_G, GPU_CLEAR_COLOR_B},
    .towards_sun_dir = APP.towards_sun_dir,
  };

  // Sun shadow map render pass
  {
    depth_target.texture = APP.gpu.shadow_tex;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, 0, 0, &depth_target);
    GPU_DrawWorld(cmd, pass, true);
    SDL_EndGPURenderPass(pass);
  }

  // World render pass
  {
    APP.gpu.world_uniform.camera_transform = APP.camera_transform;
    if (APP.debug.sun_camera)
    {
      APP.gpu.world_uniform.camera_transform = APP.sun_camera_transform;
      APP.gpu.world_uniform.camera_position = APP.sun_camera_p;
    }

    depth_target.texture = APP.gpu.tex_depth;

    SDL_GPUColorTargetInfo color_target =
    {
      .clear_color =
      {
        .r = GPU_CLEAR_COLOR_R,
        .g = GPU_CLEAR_COLOR_G,
        .b = GPU_CLEAR_COLOR_B,
        .a = GPU_CLEAR_COLOR_A,
      },
      .load_op = SDL_GPU_LOADOP_CLEAR,
    };

    if (APP.gpu.tex_msaa)
    {
      color_target.store_op = SDL_GPU_STOREOP_RESOLVE;
      color_target.texture = APP.gpu.tex_msaa;
      color_target.resolve_texture = APP.gpu.tex_resolve;
      color_target.cycle = true;
      color_target.cycle_resolve_texture = true;
    }
    else
    {
      color_target.load_op = SDL_GPU_LOADOP_CLEAR;
      color_target.store_op = SDL_GPU_STOREOP_STORE;
      color_target.texture = swapchain_tex;
    }

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
    GPU_DrawWorld(cmd, pass, false);
    SDL_EndGPURenderPass(pass);
  }

  if (APP.gpu.sample_count > SDL_GPU_SAMPLECOUNT_1)
  {
    SDL_GPUBlitInfo blit =
    {
      .source =
      {
        .texture = APP.gpu.tex_resolve,
        .w = APP.window_width,
        .h = APP.window_height,
      },
      .destination =
      {
        .texture = swapchain_tex,
        .w = draw_width,
        .h = draw_height,
      },
      .load_op = SDL_GPU_LOADOP_DONT_CARE,
      .filter = SDL_GPU_FILTER_LINEAR,
    };
    SDL_BlitGPUTexture(cmd, &blit);
  }

  // UI render pass
  if (APP.gpu.ui.indices_count)
  {
    // Upload uniform
    UI_Uniform uniform =
    {
      .window_dim = APP.window_dim,
      .texture_dim = (V2){APP.font.texture_dim, APP.font.texture_dim},
    };
    GPU_UpdateUIUniform(cmd, uniform);

    // Upload buffers to GPU
    {
      U32 transfer_size = sizeof(APP.gpu.ui.indices[0]) * APP.gpu.ui.indices_count;
      GPU_TransferBuffer(APP.gpu.ui.gpu.indices, APP.gpu.ui.indices, transfer_size);
    }
    if (APP.gpu.ui.shapes_count)
    {
      U32 transfer_size = sizeof(APP.gpu.ui.shapes[0]) * APP.gpu.ui.shapes_count;
      GPU_TransferBuffer(APP.gpu.ui.gpu.shape_buffer, APP.gpu.ui.shapes, transfer_size);
    }
    if (APP.gpu.ui.clips_count)
    {
      U32 transfer_size = sizeof(APP.gpu.ui.clips[0]) * APP.gpu.ui.clips_count;
      GPU_TransferBuffer(APP.gpu.ui.gpu.clip_buffer, APP.gpu.ui.clips, transfer_size);
    }

    // Start pass
    SDL_GPUColorTargetInfo color_target =
    {
      .load_op = SDL_GPU_LOADOP_LOAD,
      .store_op = SDL_GPU_STOREOP_STORE,
      .texture = swapchain_tex,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, 0);

    // Bind texture & sampler
    SDL_GPUTextureSamplerBinding sampler_binding =
    {
      .texture = APP.gpu.ui.gpu.atlas_texture,
      .sampler = APP.gpu.ui.gpu.atlas_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &sampler_binding, 1);

    // Bind pipeline
    SDL_BindGPUGraphicsPipeline(pass, APP.gpu.ui_pipeline);

    // Bind storage buffers
    SDL_GPUBuffer *storage_bufs[2] =
    {
      APP.gpu.ui.gpu.shape_buffer,
      APP.gpu.ui.gpu.clip_buffer,
    };
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage_bufs, ArrayCount(storage_bufs));

    // Bind index buffer
    SDL_GPUBufferBinding binding_ind = { .buffer = APP.gpu.ui.gpu.indices };
    SDL_BindGPUIndexBuffer(pass, &binding_ind, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // Draw
    SDL_DrawGPUIndexedPrimitives(pass, APP.gpu.ui.indices_count, 1, 0, 0, 0);

    // End pass
    SDL_EndGPURenderPass(pass);
  }

  SDL_SubmitGPUCommandBuffer(cmd);
}
