#define GPU_CLEAR_DEPTH_FLOAT 1.0f
#define GPU_CLEAR_COLOR_R 0.78f
#define GPU_CLEAR_COLOR_G 0.78f
#define GPU_CLEAR_COLOR_B 0.96f
#define GPU_CLEAR_COLOR_A 1.0f
#define GPU_JOINT_TRANSFORMS_MAX_SIZE (sizeof(Mat4)*62)
#define GPU_SHADOW_MAP_DIM 2048

static U32 GPU_MipMapCount(U32 width, U32 height)
{
  U32 max_dim = Max(width, height);
  I32 msb = MostSignificantBitU32(max_dim);
  if (msb < 0) return 0;
  return msb;
}

static SDL_GPUBuffer *GPU_CreateBuffer(SDL_GPUBufferUsageFlags usage, U32 size, const char *name)
{
  SDL_GPUBufferCreateInfo desc = {.usage = usage, .size = size};
  SDL_GPUBuffer *result = SDL_CreateGPUBuffer(APP.gpu.device, &desc);
  Assert(result);
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

static void GPU_ProcessWindowResize()
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

    APP.gpu.tex_depth = GPU_CreateDepthTexture(draw_width, draw_height, false);
    APP.gpu.tex_msaa = GPU_CreateMSAATexture(draw_width, draw_height);
    APP.gpu.tex_resolve = GPU_CreateResolveTexture(draw_width, draw_height);
  }

  APP.gpu.draw_width = draw_width;
  APP.gpu.draw_height = draw_height;
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

static void GPU_InitModelBuffers(bool is_skinned, U32 model_index)
{
  void *vertices = 0;
  U32 vertices_size = 0;
  U16 *indices = 0;
  U32 indices_size = 0;
  const char *vrt_buf_name = "";
  const char *ind_buf_name = "";
  const char *inst_buf_name = "";
  U32 instances_size = (is_skinned ?
                        RD_MAX_SKINNED_INSTANCES * sizeof(RDR_SkinnedInstance) :
                        RD_MAX_RIGID_INSTANCES * sizeof(RDR_RigidInstance));

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

  GPU_ModelBuffers *models_base = (is_skinned ? APP.gpu.skinneds : APP.gpu.rigids);
  GPU_ModelBuffers *model = models_base + model_index;
  model->ind_count = indices_size / sizeof(U16);

  // create model vertex buffer
  model->vert_buf = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertices_size, vrt_buf_name);
  Assert(model->vert_buf); // @todo report err

  // create model index buffer
  model->ind_buf = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX, indices_size, ind_buf_name);
  Assert(model->ind_buf); // @todo report err

  // create model per instance storage buffer
  model->inst_buf = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, instances_size, inst_buf_name);
  Assert(model->inst_buf); // @todo report err

  if (is_skinned)
  {
    APP.gpu.skinned_pose_bufs[model_index] =
      GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                       RD_MAX_SKINNED_INSTANCES * sizeof(RDR_SkinnedPose),
                       "skinned_pose_buf (todo: add model name)");
  }

  GPU_TransferBuffer(model->vert_buf, vertices, vertices_size);
  GPU_TransferBuffer(model->ind_buf, indices, indices_size);
}

static SDL_GPUTexture *GPU_CreateAndLoadTexture2DArray(const char **paths, U64 path_count,
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
    .num_levels = GPU_MipMapCount(imgs[0]->w, imgs[0]->h),
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
    GPU_TransferTexture(result,
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

static void GPU_LoadTextureFiles()
{
#define LoadTexturesFromAmbientCgCom(Name) do{ \
const char *paths[] = \
{ \
"../res/tex/" #Name "/" #Name "_1K-JPG_Color.jpg", \
"../res/tex/" #Name "/" #Name "_1K-JPG_NormalGL.jpg", \
"../res/tex/" #Name "/" #Name "_1K-JPG_Roughness.jpg", \
"../res/tex/" #Name "/" #Name "_1K-JPG_Displacement.jpg", \
}; \
APP.gpu.wall_texs[TEX_##Name] = GPU_CreateAndLoadTexture2DArray(paths, ArrayCount(paths), #Name); \
}while(0)

  LoadTexturesFromAmbientCgCom(Bricks071);
  LoadTexturesFromAmbientCgCom(Bricks097);
  LoadTexturesFromAmbientCgCom(Grass004);
  LoadTexturesFromAmbientCgCom(Ground037);
  LoadTexturesFromAmbientCgCom(Ground068);
  LoadTexturesFromAmbientCgCom(Ground078);
  LoadTexturesFromAmbientCgCom(Leather011);
  LoadTexturesFromAmbientCgCom(PavingStones067);
  LoadTexturesFromAmbientCgCom(Tiles101);

  {
    const char *paths[] =
    {
      "../res/tex/TestPBR001/TestPBR001_Color.png",
      "../res/tex/TestPBR001/TestPBR001_NormalGL.png",
      "../res/tex/TestPBR001/TestPBR001_Roughness.jpg",
      "../res/tex/TestPBR001/TestPBR001_Displacement.jpg",
    };
    APP.gpu.wall_texs[TEX_TestPBR001] = GPU_CreateAndLoadTexture2DArray(paths, ArrayCount(paths), "TestPBR001");
  }
}

static void GPU_ModifyPipelineForShadowMapping(SDL_GPUGraphicsPipelineCreateInfo *pipeline)
{
  pipeline->multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  pipeline->rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
}

static void GPU_Init()
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
      GPU_InitModelBuffers(false, model_index);
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
        .num_samplers = 1,
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
      GPU_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(RDR_RigidVertex),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 3,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .location = 0,
          .offset = 0,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = sizeof(float)*4,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 2,
          .offset = sizeof(float)*7,
        },
      },
    };


    APP.gpu.pipelines[0].rigid = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.pipelines[0].rigid);

    GPU_ModifyPipelineForShadowMapping(&pipeline);
    APP.gpu.pipelines[1].rigid = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.pipelines[1].rigid);

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  // Skinned pipeline
  {
    ForU32(model_index, RdrSkinned_COUNT)
    {
      GPU_InitModelBuffers(true, model_index);
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
        .num_samplers = 1,
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
      GPU_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(RDR_SkinnedVertex),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 5,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .location = 0,
          .offset = 0,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = sizeof(float)*4,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 2,
          .offset = sizeof(float)*7,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 3,
          .offset = sizeof(float)*7 + sizeof(U32),
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .location = 4,
          .offset = sizeof(float)*7 + sizeof(U32)*2,
        },
      },
    };


    APP.gpu.pipelines[0].skinned = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.pipelines[0].skinned);

    GPU_ModifyPipelineForShadowMapping(&pipeline);
    APP.gpu.pipelines[1].skinned = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.pipelines[1].skinned);

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  GPU_LoadTextureFiles();

  // Wall pipeline
  {
    // create wall vertex buffers
    ForArray(i, APP.gpu.wall_vert_buffers)
    {
      APP.gpu.wall_vert_buffers[i] =
        GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                         sizeof(APP.rdr.wall_mesh_buffers[i].verts),
                         "Wall vertex buffer (one of many)");
    }

    {
      RDR_RigidInstance inst = {};
      inst.transform = Mat4_Identity();
      inst.color = Color32_RGBf(1,1,1);
      APP.gpu.wall_inst_buf = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                                               sizeof(inst),
                                               "Wall instance buffer");
      GPU_TransferBuffer(APP.gpu.wall_inst_buf, &inst, sizeof(inst));
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
        .num_samplers = 2,
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
      GPU_DefaultPipeline(&color_desc, vertex_shader, fragment_shader);

    pipeline.vertex_input_state = (SDL_GPUVertexInputState)
    {
      .num_vertex_buffers = 1,
      .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
      {
        {
          .slot = 0,
          .pitch = sizeof(RDR_WallVertex),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0,
        },
      },
      .num_vertex_attributes = 4,
      .vertex_attributes = (SDL_GPUVertexAttribute[])
      {
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .location = 0,
          .offset = 0,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .location = 1,
          .offset = sizeof(float)*4,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .location = 2,
          .offset = sizeof(float)*7,
        },
        {
          .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
          .location = 3,
          .offset = sizeof(float)*9,
        },
      },
    };


    APP.gpu.pipelines[0].wall = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.pipelines[0].wall);

    GPU_ModifyPipelineForShadowMapping(&pipeline);
    APP.gpu.pipelines[1].wall = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
    Assert(APP.gpu.pipelines[1].wall);

    SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
    SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
  }

  // shadow map
  {
    APP.gpu.shadow_tex = GPU_CreateDepthTexture(GPU_SHADOW_MAP_DIM, GPU_SHADOW_MAP_DIM, true);

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

  // font atlas
  {
    SDL_GPUTextureCreateInfo tex_info =
    {
      .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .width = FONT_ATLAS_DIM,
      .height = FONT_ATLAS_DIM,
      .layer_count_or_depth = FONT_ATLAS_LAYERS,
      .num_levels = GPU_MipMapCount(FONT_ATLAS_DIM, FONT_ATLAS_DIM),
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
    };
    APP.gpu.font_atlas_tex = SDL_CreateGPUTexture(APP.gpu.device, &tex_info);
    SDL_SetGPUTextureName(APP.gpu.device, APP.gpu.font_atlas_tex, "Font Atlas Texture");
  }

  GPU_ProcessWindowResize();
}

static void GPU_Deinit()
{
  ForArray(i, APP.gpu.pipelines)
  {
    SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.pipelines[i].rigid);
    SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.pipelines[i].skinned);
    SDL_ReleaseGPUGraphicsPipeline(APP.gpu.device, APP.gpu.pipelines[i].wall);
  }

  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_depth);
  if (APP.gpu.tex_msaa)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_msaa);
  if (APP.gpu.tex_resolve)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.tex_resolve);

  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.shadow_tex);
  SDL_ReleaseGPUSampler(APP.gpu.device, APP.gpu.shadow_sampler);

  SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.font_atlas_tex);

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

  ForArray(i, APP.gpu.wall_vert_buffers)
    SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.wall_vert_buffers[i]);

  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.gpu.wall_inst_buf);
  SDL_ReleaseGPUSampler(APP.gpu.device, APP.gpu.wall_sampler);
  ForArray(i, APP.gpu.wall_texs)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.wall_texs[i]);
}

static void GPU_DrawModelBuffers(SDL_GPURenderPass *pass,
                                 GPU_ModelBuffers *model, U32 instance_count,
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

static void GPU_RenderPassDrawCalls(SDL_GPURenderPass *pass, U32 pipeline_index)
{
  AssertBounds(pipeline_index, APP.gpu.pipelines);

  // bind fragment shadow texture sampler
  {
    SDL_GPUTextureSamplerBinding binding_sampl =
    {
      .texture = APP.gpu.shadow_tex,
      .sampler = APP.gpu.shadow_sampler,
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &binding_sampl, 1);
  }

  // walls
  SDL_BindGPUGraphicsPipeline(pass, APP.gpu.pipelines[pipeline_index].wall);
  {
    // bind instance storage buffer
    SDL_BindGPUVertexStorageBuffers(pass, 0, &APP.gpu.wall_inst_buf, 1);

    ForArray(mesh_index, APP.rdr.wall_mesh_buffers)
    {
      RDR_WallMeshBuffer *buf = APP.rdr.wall_mesh_buffers + mesh_index;
      if (buf->vert_count)
      {
        // bind vertex buffer
        AssertBounds(mesh_index, APP.gpu.wall_vert_buffers);
        SDL_GPUBufferBinding binding_vrt = { .buffer = APP.gpu.wall_vert_buffers[mesh_index] };
        SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);

        AssertBounds(mesh_index, APP.gpu.wall_texs);
        SDL_GPUTexture *texture = APP.gpu.wall_texs[mesh_index];

        // bind fragment color texture sampler
        SDL_GPUTextureSamplerBinding binding_sampl =
        {
          .texture = texture,
          .sampler = APP.gpu.wall_sampler,
        };
        SDL_BindGPUFragmentSamplers(pass, 1, &binding_sampl, 1);

        SDL_DrawGPUPrimitives(pass, buf->vert_count, 1, 0, 0);
      }
    }
  }

  // rigid
  SDL_BindGPUGraphicsPipeline(pass, APP.gpu.pipelines[pipeline_index].rigid);
  ForArray(i, APP.gpu.rigids)
  {
    GPU_ModelBuffers *model = APP.gpu.rigids + i;
    RDR_Rigid *rigid = APP.rdr.rigids + i;
    GPU_DrawModelBuffers(pass, model, rigid->instance_count, 0);
  }

  // skinned
  SDL_BindGPUGraphicsPipeline(pass, APP.gpu.pipelines[pipeline_index].skinned);
  ForArray(i, APP.gpu.skinneds)
  {
    GPU_ModelBuffers *model = APP.gpu.skinneds + i;
    RDR_Skinned *skinned = APP.rdr.skinneds + i;

    SDL_GPUBuffer *pose_buf = APP.gpu.skinned_pose_bufs[i];
    GPU_DrawModelBuffers(pass, model, skinned->instance_count, pose_buf);
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
    /* Swapchain is unavailable, cancel work */
    SDL_CancelGPUCommandBuffer(cmd);
    return;
  }

  GPU_ProcessWindowResize();

  // upload wall vertices
  static_assert(ArrayCount(APP.gpu.wall_vert_buffers) == ArrayCount(APP.rdr.wall_mesh_buffers));
  ForArray(i, APP.rdr.wall_mesh_buffers)
  {
    RDR_WallMeshBuffer *buf = APP.rdr.wall_mesh_buffers + i;
    if (buf->vert_count)
    {
      U32 transfor_size = buf->vert_count * sizeof(buf->verts[0]);
      GPU_TransferBuffer(APP.gpu.wall_vert_buffers[i], buf->verts, transfor_size);
    }
  }

  // upload model instance data
  ForArray(i, APP.gpu.rigids)
  {
    GPU_ModelBuffers *gpu_model = APP.gpu.rigids + i;
    RDR_Rigid *rigid = APP.rdr.rigids + i;
    if (rigid->instance_count)
    {
      GPU_TransferBuffer(gpu_model->inst_buf,
                         rigid->instances,
                         rigid->instance_count * sizeof(rigid->instances[0]));
    }
  }
  ForArray(i, APP.gpu.skinneds)
  {
    GPU_ModelBuffers *gpu_model = APP.gpu.skinneds + i;
    RDR_Skinned *skinned = APP.rdr.skinneds + i;
    SDL_GPUBuffer *pose_buf = APP.gpu.skinned_pose_bufs[i];

    if (skinned->instance_count)
    {
      GPU_TransferBuffer(gpu_model->inst_buf,
                         skinned->instances,
                         skinned->instance_count * sizeof(skinned->instances[0]));

      GPU_TransferBuffer(pose_buf,
                         skinned->poses,
                         skinned->instance_count * sizeof(skinned->poses[0]));
    }
  }

  SDL_GPUDepthStencilTargetInfo depth_target =
  {
    .clear_depth = GPU_CLEAR_DEPTH_FLOAT,
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_DONT_CARE,
    .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
    .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
    .cycle = true,
  };

  RDR_Uniform uniform =
  {
    .camera_transform = APP.sun_camera_transform,
    .shadow_transform = APP.sun_camera_transform,
    .camera_position = APP.camera_p,
    .background_color = (V3){GPU_CLEAR_COLOR_R, GPU_CLEAR_COLOR_G, GPU_CLEAR_COLOR_B},
    .towards_sun_dir = APP.towards_sun_dir,
  };

  // Sun shadow map render pass
  {
    SDL_PushGPUVertexUniformData(cmd, 0, &uniform, sizeof(uniform));
    SDL_PushGPUFragmentUniformData(cmd, 0, &uniform, sizeof(uniform));

    depth_target.texture = APP.gpu.shadow_tex;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, 0, 0, &depth_target);
    GPU_RenderPassDrawCalls(pass, 1);
    SDL_EndGPURenderPass(pass);
  }

  // Final render pass
  {
    uniform.camera_transform = APP.camera_transform;
    if (APP.debug.sun_camera)
    {
      uniform.camera_transform = APP.sun_camera_transform;
      uniform.camera_position = APP.sun_camera_p;
    }
    SDL_PushGPUVertexUniformData(cmd, 0, &uniform, sizeof(uniform));
    SDL_PushGPUFragmentUniformData(cmd, 0, &uniform, sizeof(uniform));

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

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
    GPU_RenderPassDrawCalls(pass, 0);
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
