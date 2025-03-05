#define GPU_CLEAR_DEPTH_FLOAT 1.0f
#define GPU_CLEAR_COLOR_R 0.78f
#define GPU_CLEAR_COLOR_G 0.78f
#define GPU_CLEAR_COLOR_B 0.96f
#define GPU_CLEAR_COLOR_A 1.0f
#define GPU_JOINT_TRANSFORMS_MAX_SIZE (sizeof(Mat4)*62)

static SDL_GPUBuffer *Gpu_CreateBuffer(SDL_GPUBufferUsageFlags usage, U32 size, const char *name)
{
  SDL_GPUBufferCreateInfo desc = {.usage = usage, .size = size};
  SDL_GPUBuffer *result = SDL_CreateGPUBuffer(APP.gpu.device, &desc);
  Assert(result);
  SDL_SetGPUBufferName(APP.gpu.device, result, name);
  return result;
}

static SDL_GPUTexture *Gpu_CreateDepthTexture(U32 width, U32 height)
{
  SDL_GPUTextureCreateInfo createinfo = {
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

  SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);

  Assert(result); // @todo report err
  return result;
}

static SDL_GPUTexture *Gpu_CreateMSAATexture(U32 width, U32 height)
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

static SDL_GPUTexture *Gpu_CreateResolveTexture(U32 width, U32 height)
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

static void Gpu_ProcessWindowResize()
{
  U32 draw_width, draw_height;
  SDL_GetWindowSizeInPixels(APP.window, (int *)&draw_width, (int *)&draw_height);

  if (APP.gpu.draw_width != draw_width ||
      APP.gpu.draw_height != draw_height)
  {
    if (APP.gpu.tex_depth)
      SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_depth);
    if (APP.gpu.tex_msaa)
      SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_msaa);
    if (APP.gpu.tex_resolve)
      SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_resolve);

    APP.gpu.tex_depth = Gpu_CreateDepthTexture(draw_width, draw_height);
    APP.gpu.tex_msaa = Gpu_CreateMSAATexture(draw_width, draw_height);
    APP.gpu.tex_resolve = Gpu_CreateResolveTexture(draw_width, draw_height);
  }

  APP.gpu.draw_width = draw_width;
  APP.gpu.draw_height = draw_height;
}

static void Gpu_TransferBuffer(SDL_GPUBuffer *gpu_buffer, void *data, U32 data_size)
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

static void Gpu_TransferTexture(SDL_GPUTexture *gpu_tex,
                                U32 layer, U32 w, U32 h,
                                void *data, U32 data_size)
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

    SDL_GPUTextureTransferInfo trans_info = {
      .transfer_buffer = buf_transfer,
      .offset = 0,
    };
    SDL_GPUTextureRegion dst_region = {
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

  SDL_ReleaseGPUTransferBuffer(APP.gpu.device, buf_transfer);
}


static SDL_GPUGraphicsPipelineCreateInfo Gpu_DefaultPipeline(SDL_GPUColorTargetDescription *color,
                                                             SDL_GPUShader *vertex,
                                                             SDL_GPUShader *fragment)
{
  SDL_GPUGraphicsPipelineCreateInfo pipeline =
  {
    .target_info =
    {
      .num_color_targets = 1,
      .color_target_descriptions = color,
      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      .has_depth_stencil_target = true,
    },
    .depth_stencil_state =
    {
      .enable_depth_test = true,
      .enable_depth_write = true,
      .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
    },
    .multisample_state =
    {
      .sample_count = APP.gpu.sample_count,
    },
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .vertex_shader = vertex,
    .fragment_shader = fragment,
    .props = 0
  };
  return pipeline;
}

static void Gpu_InitModelBuffers(bool is_skinned, U32 model_index)
{
  void *vertices = 0;
  U32 vertices_size = 0;
  U16 *indices = 0;
  U32 indices_size = 0;
  const char *vrt_buf_name = "";
  const char *ind_buf_name = "";
  const char *inst_buf_name = "";
  U32 instances_size = (is_skinned ?
                        RD_MAX_SKINNED_INSTANCES * sizeof(Rdr_SkinnedInstance) :
                        RD_MAX_RIGID_INSTANCES * sizeof(Rdr_RigidInstance));

  if (is_skinned)
  {
    switch (model_index)
    {
      default: Assert(0); break;
      case RdrSkinned_Worker:
      {
        vertices = Model_Worker_vrt;
        vertices_size = sizeof(Model_Worker_vrt);
        indices = Model_Worker_ind;
        indices_size = sizeof(Model_Worker_ind);
        vrt_buf_name = "Worker skinned vrt buf";
        ind_buf_name = "Worker skinned ind buf";
        inst_buf_name = "Worker skinned inst buf";
      } break;
    }
  }
  else
  {
    switch (model_index)
    {
      default: Assert(0); break;
      case RdrRigid_Teapot:
      {
        vertices = Model_teapot_vrt;
        vertices_size = sizeof(Model_teapot_vrt);
        indices = Model_teapot_ind;
        indices_size = sizeof(Model_teapot_ind);
        vrt_buf_name = "Teapot rigid vrt buf";
        ind_buf_name = "Teapot rigid ind buf";
        inst_buf_name = "Teapot rigid inst buf";
      } break;
      case RdrRigid_Flag:
      {
        vertices = Model_flag_vrt;
        vertices_size = sizeof(Model_flag_vrt);
        indices = Model_flag_ind;
        indices_size = sizeof(Model_flag_ind);
        vrt_buf_name = "Flag rigid vrt buf";
        ind_buf_name = "Flag rigid ind buf";
        inst_buf_name = "Flag rigid inst buf";
      } break;
    }
  }

  Gpu_ModelBuffers *models_base = (is_skinned ? APP.gpu.skinneds : APP.gpu.rigids);
  Gpu_ModelBuffers *model = models_base + model_index;
  model->ind_count = indices_size / sizeof(U16);

  // create model vertex buffer
  model->vert_buf = Gpu_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertices_size, vrt_buf_name);
  Assert(model->vert_buf); // @todo report err

  // create model index buffer
  model->ind_buf = Gpu_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX, indices_size, ind_buf_name);
  Assert(model->ind_buf); // @todo report err

  // create model per instance storage buffer
  model->inst_buf = Gpu_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, instances_size, inst_buf_name);
  Assert(model->inst_buf); // @todo report err

  if (is_skinned)
  {
    APP.gpu.skinned_pose_bufs[model_index] =
      Gpu_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                       RD_MAX_SKINNED_INSTANCES * sizeof(Rdr_SkinnedPose),
                       "skinned_pose_buf (todo: add model name)");
  }

  Gpu_TransferBuffer(model->vert_buf, vertices, vertices_size);
  Gpu_TransferBuffer(model->ind_buf, indices, indices_size);
}

static SDL_GPUTexture *Gpu_CreateAndLoadTexture2DArray(const char **paths, U64 path_count,
                                                       const char *texture_name)
{
  Arena *a = APP.tmp;
  ArenaScope scope0 = Arena_PushScope(a);

  SDL_Surface **imgs = Alloc(a, SDL_Surface *, path_count);
  ForU64(i, path_count)
    imgs[i] = IMG_Load(paths[i]);

  ForU64(i, path_count)
  {
    Assert(imgs[i]); // @todo report err & use fallback textures
    Assert(imgs[0]->w == imgs[i]->w);
    Assert(imgs[0]->h == imgs[i]->h);
    Assert(imgs[0]->format == SDL_PIXELFORMAT_RGB24);
  }

  SDL_GPUTextureCreateInfo tex_info =
  {
    .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .width = imgs[0]->w,
    .height = imgs[0]->h,
    .layer_count_or_depth = path_count,
    .num_levels = 9,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
  };
  SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &tex_info);
  SDL_SetGPUTextureName(APP.gpu.device, result, texture_name);

  U32 img_size = imgs[0]->w * imgs[0]->h * 4;
  ForU64(i, path_count)
  {
    ArenaScope scope1 = Arena_PushScope(a);

    // temporary: convert rgb -> rgba
    U8 *buffer = Alloc(a, U8, img_size);
    U8 *pixels = (U8 *)imgs[i]->pixels;

    ForI32(y, imgs[i]->h)
    {
      U8 *buffer_row = buffer + y*imgs[i]->w*4;
      U8 *px_row = pixels + y*imgs[i]->pitch;
      ForI32(x, imgs[i]->w)
      {
        buffer_row[x*4 + 0] = px_row[x*3 + 0];
        buffer_row[x*4 + 1] = px_row[x*3 + 1];
        buffer_row[x*4 + 2] = px_row[x*3 + 2];
        buffer_row[x*4 + 3] = 255;
      }
    }

    // @todo these transfer could be optimized
    Gpu_TransferTexture(result,
                        i, imgs[i]->w, imgs[i]->h,
                        buffer, img_size);

    Arena_PopScope(scope1);
  }

  ForU64(i, path_count)
    SDL_DestroySurface(imgs[i]);

  Arena_PopScope(scope0);

  // Generate texture mipmap
  {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
    SDL_GenerateMipmapsForGPUTexture(cmd, result);
    SDL_SubmitGPUCommandBuffer(cmd);
  }

  return result;
}

static void Gpu_Init()
{
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
    {
      APP.gpu.sample_count = SDL_GPU_SAMPLECOUNT_4;
    }
  }

  SDL_GPUColorTargetDescription color_desc = {
    .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
  };

  // Rigid pipeline
  {
    ForU32(model_index, RdrRigid_COUNT)
    {
      Gpu_InitModelBuffers(false, model_index);
    }

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
        .code = g_ShaderRigidVS,
        .code_size = sizeof(g_ShaderRigidVS),
        .entrypoint = "ShaderRigidVS",
      };
      vertex_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUShader *fragment_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 0,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_ShaderRigidPS,
        .code_size = sizeof(g_ShaderRigidPS),
        .entrypoint = "ShaderRigidPS",
      };
      fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline =
      Gpu_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(Rdr_RigidVertex),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 3,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 0,
          .offset = 0,
        },
        {
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = sizeof(float) * 3,
        },
        {
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 2,
          .offset = sizeof(float) * 6,
        },
      },
    };

    APP.gpu.rigid_pipeline = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.rigid_pipeline); // @todo report user to error and exit program on fail

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  // Skinned pipeline
  {
    ForU32(model_index, RdrSkinned_COUNT)
    {
      Gpu_InitModelBuffers(true, model_index);
    }

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
        .code = g_ShaderSkinnedVS,
        .code_size = sizeof(g_ShaderSkinnedVS),
        .entrypoint = "ShaderSkinnedVS",
      };
      vertex_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUShader *fragment_shader = 0;
    {
      SDL_GPUShaderCreateInfo create_info =
      {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 0,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
        .num_uniform_buffers = 1,

        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .code = g_ShaderSkinnedPS,
        .code_size = sizeof(g_ShaderSkinnedPS),
        .entrypoint = "ShaderSkinnedPS",
      };
      fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline =
      Gpu_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(Rdr_SkinnedVertex),
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
          .offset = sizeof(float)*3,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 2,
          .offset = sizeof(float)*6,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 3,
          .offset = sizeof(float)*6 + sizeof(U32),
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .location = 4,
          .offset = sizeof(float)*6 + sizeof(U32)*2,
        },
      },
    };

    APP.gpu.skinned_pipeline = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.skinned_pipeline); // @todo report user to error and exit program on fail

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  // Wall pipeline
  {
    // create wall vertex buffer
    APP.gpu.wall_vert_buf = Gpu_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                             sizeof(APP.rdr.wall_verts),
                                             "Wall vertex buffer");
    {
      Rdr_RigidInstance inst = {};
      inst.transform = Mat4_Identity();
      inst.color = Color32_RGBf(1,1,1);
      APP.gpu.wall_inst_buf = Gpu_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                                               sizeof(inst),
                                               "Wall instance buffer");
      Gpu_TransferBuffer(APP.gpu.wall_inst_buf, &inst, sizeof(inst));
    }

    // Load textures
    {
      const char *paths[] =
      {
        "../res/tex/1.jpg",
        "../res/tex/2.jpg",
      };
      APP.gpu.wall_tex = Gpu_CreateAndLoadTexture2DArray(paths, ArrayCount(paths), "Terrain tex");
    }

    {
      const char *paths[] =
      {
        "../res/tex/PavingStones067/PavingStones067_1K-JPG_Color.jpg",
        "../res/tex/PavingStones067/PavingStones067_1K-JPG_NormalDX.jpg",
        "../res/tex/PavingStones067/PavingStones067_1K-JPG_Roughness.jpg",
        "../res/tex/PavingStones067/PavingStones067_1K-JPG_Displacement.jpg",
        "../res/tex/PavingStones067/PavingStones067_1K-JPG_AmbientOcclusion.jpg",
      };
      APP.gpu.wall_pbr_tex = Gpu_CreateAndLoadTexture2DArray(paths, ArrayCount(paths), "Terrain PBR tex");
    }

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
      APP.gpu.wall_sampler = SDL_CreateGPUSampler(APP.gpu.device, &sampler_info);
    }

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
        .code = g_ShaderWallVS,
        .code_size = sizeof(g_ShaderWallVS),
        .entrypoint = "ShaderWallVS",
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
        .code = g_ShaderWallPS,
        .code_size = sizeof(g_ShaderWallPS),
        .entrypoint = "ShaderWallPS",
      };
      fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline =
      Gpu_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(Rdr_WallVertex),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 4,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 0,
          .offset = 0,
        },
        {
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = sizeof(float) * 3,
        },
        {
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 2,
          .offset = sizeof(float) * 6,
        },
        {
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 3,
          .offset = sizeof(float) * 9,
        },
      },
    };

    APP.gpu.wall_pipeline = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.wall_pipeline); // @todo report error to user and exit program on fail

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  Gpu_ProcessWindowResize();
}

static void Gpu_Deinit()
{
  SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.rigid_pipeline);
  SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.skinned_pipeline);
  SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.wall_pipeline);

  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_depth);
  if (APP.gpu.tex_msaa)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_msaa);
  if (APP.gpu.tex_resolve)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_resolve);

  ForArray(i, APP.gpu.rigids)
  {
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.rigids[i].vert_buf);
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.rigids[i].ind_buf);
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.rigids[i].inst_buf);
  }
  ForArray(i, APP.gpu.skinneds)
  {
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.skinneds[i].vert_buf);
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.skinneds[i].ind_buf);
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.skinneds[i].inst_buf);
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.skinned_pose_bufs[i]);
  }

  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.wall_vert_buf);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.wall_inst_buf);
  SDL_ReleaseGPUSampler(APP.gpu.device, APP.gpu.wall_sampler);
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.wall_tex);
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.wall_pbr_tex);
}

static void Gpu_DrawModelBuffers(SDL_GPURenderPass *pass,
                                 Gpu_ModelBuffers *model, U32 instance_count,
                                 SDL_GPUBuffer *additional_storage_buf)
{
  if (!instance_count)
    return;

  // bind vertex buffer
  SDL_GPUBufferBinding binding_vrt = { .buffer = model->vert_buf };
  SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);

  // bind index buffer
  SDL_GPUBufferBinding binding_ind = { .buffer = model->ind_buf };
  SDL_BindGPUIndexBuffer(pass, &binding_ind, SDL_GPU_INDEXELEMENTSIZE_16BIT);

  // bind instance storage buffer
  SDL_GPUBuffer *storage_bufs[2] = {
    model->inst_buf,
    additional_storage_buf
  };
  U32 storage_bufs_count = (additional_storage_buf ? 2 : 1);
  SDL_BindGPUVertexStorageBuffers(pass, 0, storage_bufs, storage_bufs_count);

  SDL_DrawGPUIndexedPrimitives(pass, model->ind_count, instance_count, 0, 0, 0);
}

static void Gpu_Iterate()
{
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
  Assert(cmd); // @todo report err

  SDL_GPUTexture *swapchain_tex = 0;
  U32 draw_width, draw_height;
  bool res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd, APP.window, &swapchain_tex, &draw_width, &draw_height);
  Assert(res); // @todo report err

  if (!swapchain_tex)
  {
    /* Swapchain is unavailable, cancel work */
    SDL_CancelGPUCommandBuffer(cmd);
    return;
  }

  Gpu_ProcessWindowResize();

  // upload wall vertices
  if (APP.rdr.wall_vert_count)
  {
    Gpu_TransferBuffer(APP.gpu.wall_vert_buf, APP.rdr.wall_verts, sizeof(APP.rdr.wall_verts));
  }

  // upload model instance data
  ForArray(i, APP.gpu.rigids)
  {
    Gpu_ModelBuffers *gpu_model = APP.gpu.rigids + i;
    Rdr_Rigid *rigid = APP.rdr.rigids + i;
    if (rigid->instance_count)
    {
      Gpu_TransferBuffer(gpu_model->inst_buf,
                         rigid->instances,
                         rigid->instance_count * sizeof(rigid->instances[0]));
    }
  }
  ForArray(i, APP.gpu.skinneds)
  {
    Gpu_ModelBuffers *gpu_model = APP.gpu.skinneds + i;
    Rdr_Skinned *skinned = APP.rdr.skinneds + i;
    SDL_GPUBuffer *pose_buf = APP.gpu.skinned_pose_bufs[i];

    if (skinned->instance_count)
    {
      Gpu_TransferBuffer(gpu_model->inst_buf,
                         skinned->instances,
                         skinned->instance_count * sizeof(skinned->instances[0]));

      Gpu_TransferBuffer(pose_buf,
                         skinned->poses,
                         skinned->instance_count * sizeof(skinned->poses[0]));
    }
  }

  // transform uniforms
  {
    struct
    {
      Mat4 CameraTransform;
      V3 CameraPosition;
      float pad0;
      V3 BackgroundColor;
      float pad1;
    } uniform = {};

    uniform.CameraTransform = APP.camera_all_mat;
    uniform.CameraPosition = APP.camera_p;
    uniform.BackgroundColor = (V3){GPU_CLEAR_COLOR_R, GPU_CLEAR_COLOR_G, GPU_CLEAR_COLOR_B};

    SDL_PushGPUVertexUniformData(cmd, 0, &uniform, sizeof(uniform));
  }

  SDL_GPUDepthStencilTargetInfo depth_target = {
    .clear_depth = GPU_CLEAR_DEPTH_FLOAT,
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_DONT_CARE,
    .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
    .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
    .texture = APP.gpu.tex_depth,
    .cycle = true,
  };

  SDL_GPUColorTargetInfo color_target =
  {
    .clear_color =
    {
      .r = GPU_CLEAR_COLOR_R,
      .g = GPU_CLEAR_COLOR_G,
      .b = GPU_CLEAR_COLOR_B,
      .a = GPU_CLEAR_COLOR_A,
    },
  };
  if (APP.gpu.tex_msaa)
  {
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
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

  // render pass
  {
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);

    // walls
    SDL_BindGPUGraphicsPipeline(pass, APP.gpu.wall_pipeline);
    {
      // bind vertex buffer
      SDL_GPUBufferBinding binding_vrt = { .buffer = APP.gpu.wall_vert_buf };
      SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);

      // bind instance storage buffer
      SDL_BindGPUVertexStorageBuffers(pass, 0, &APP.gpu.wall_inst_buf, 1);

      // bind fragment sampler
      SDL_GPUTextureSamplerBinding binding_sampl =
      {
        .texture = APP.gpu.wall_tex,
        .sampler = APP.gpu.wall_sampler,
      };
      SDL_BindGPUFragmentSamplers(pass, 0, &binding_sampl, 1);

      SDL_DrawGPUPrimitives(pass, APP.rdr.wall_vert_count, 1, 0, 0);
    }

    // rigid
    SDL_BindGPUGraphicsPipeline(pass, APP.gpu.rigid_pipeline);
    ForArray(i, APP.gpu.rigids)
    {
      Gpu_ModelBuffers *model = APP.gpu.rigids + i;
      Rdr_Rigid *rigid = APP.rdr.rigids + i;
      Gpu_DrawModelBuffers(pass, model, rigid->instance_count, 0);
    }

    // skinned
    SDL_BindGPUGraphicsPipeline(pass, APP.gpu.skinned_pipeline);
    ForArray(i, APP.gpu.skinneds)
    {
      Gpu_ModelBuffers *model = APP.gpu.skinneds + i;
      Rdr_Skinned *skinned = APP.rdr.skinneds + i;

      SDL_GPUBuffer *pose_buf = APP.gpu.skinned_pose_bufs[i];
      Gpu_DrawModelBuffers(pass, model, skinned->instance_count, pose_buf);
    }

    SDL_EndGPURenderPass(pass);
  }

  if (APP.gpu.sample_count > SDL_GPU_SAMPLECOUNT_1)
  {
    SDL_GPUBlitInfo blit =
    {
      .source =
      {
        .texture = APP.gpu.tex_resolve,
        .w = APP.gpu.draw_width,
        .h = APP.gpu.draw_height,
      },
      .destination =
      {
        .texture = swapchain_tex,
        .w = APP.gpu.draw_width,
        .h = APP.gpu.draw_height,
      },
      .load_op = SDL_GPU_LOADOP_DONT_CARE,
      .filter = SDL_GPU_FILTER_LINEAR,
    };
    SDL_BlitGPUTexture(cmd, &blit);
  }

  SDL_SubmitGPUCommandBuffer(cmd);
}
