#define GPU_DEPTH_CLEAR_FLOAT 1.0f

static SDL_GPUTexture *Gpu_CreateDepthTexture(U32 width, U32 height)
{
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetFloatProperty(props,
                         SDL_PROP_GPU_CREATETEXTURE_D3D12_CLEAR_DEPTH_FLOAT,
                         GPU_DEPTH_CLEAR_FLOAT);

    SDL_GPUTextureCreateInfo createinfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = APP.gpu.sample_count,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .props = props,
    };

    SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);

    // These properties shouldn't be destroyed.
    // SDL_DestroyProperties(props);
    // They are used again when this depth texture is "cycled".

    Assert(result); // @todo report err
    return result;
}

static SDL_GPUTexture *Gpu_CreateMSAATexture(U32 width, U32 height)
{
    if (APP.gpu.sample_count == SDL_GPU_SAMPLECOUNT_1)
    {
        return 0;
    }

    SDL_GPUTextureCreateInfo createinfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = APP.gpu.sample_count,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .props = 0,
    };

    SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);
    Assert(result); // @todo report err
    return result;
}

static SDL_GPUTexture *Gpu_CreateResolveTexture(U32 width, U32 height)
{
    if (APP.gpu.sample_count == SDL_GPU_SAMPLECOUNT_1)
    {
        return 0;
    }

    SDL_GPUTextureCreateInfo createinfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .props = 0,
    };

    SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);
    Assert(result); // @todo report err
    return result;
}

static void Gpu_ProcessWindowResize()
{
    U32 draw_width, draw_height;
    SDL_GetWindowSizeInPixels(APP.window, (int *)&draw_width, (int *)&draw_height);

    if (APP.gpu.win_state.draw_width != draw_width ||
        APP.gpu.win_state.draw_height != draw_height)
    {
        if (APP.gpu.win_state.tex_depth)
            SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.win_state.tex_depth);
        if (APP.gpu.win_state.tex_msaa)
            SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.win_state.tex_msaa);
        if (APP.gpu.win_state.tex_resolve)
            SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.win_state.tex_resolve);

        APP.gpu.win_state.tex_depth = Gpu_CreateDepthTexture(draw_width, draw_height);
        APP.gpu.win_state.tex_msaa = Gpu_CreateMSAATexture(draw_width, draw_height);
        APP.gpu.win_state.tex_resolve = Gpu_CreateResolveTexture(draw_width, draw_height);
    }

    APP.gpu.win_state.draw_width = draw_width;
    APP.gpu.win_state.draw_height = draw_height;
}

static void Gpu_TransferBuffer(SDL_GPUBuffer *gpu_buffer, void *data, U64 data_size)
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
                                void *data, U64 data_size)
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


static SDL_GPUGraphicsPipelineCreateInfo Gpu_DefaultSDLPipeline(SDL_GPUColorTargetDescription *color,
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

static void Gpu_InitModelBuffers(Rdr_ModelType model_type)
{
    Rdr_ModelVertex *vertices = 0;
    U64 vertices_size = 0;
    U16 *indices = 0;
    U64 indices_size = 0;
    const char *vrt_buf_name = "";
    const char *ind_buf_name = "";
    const char *inst_buf_name = "";

    switch (model_type)
    {
        default: Assert(0); break;
        case RdrModel_Teapot: {
            vertices = Model_teapot_vrt;
            vertices_size = sizeof(Model_teapot_vrt);
            indices = Model_teapot_ind;
            indices_size = sizeof(Model_teapot_ind);
            vrt_buf_name = "Teapot vrt buf";
            ind_buf_name = "Teapot ind buf";
            inst_buf_name = "Teapot inst buf";
        } break;
        case RdrModel_Flag: {
            vertices = Model_flag_vrt;
            vertices_size = sizeof(Model_flag_vrt);
            indices = Model_flag_ind;
            indices_size = sizeof(Model_flag_ind);
            vrt_buf_name = "Flag vrt buf";
            ind_buf_name = "Flag ind buf";
            inst_buf_name = "Flag inst buf";
        } break;
    }

    Gpu_ModelBuffers *model = APP.gpu.models + model_type;
    model->ind_count = indices_size / sizeof(U16);

    // create model vertex buffer
    {
        SDL_GPUBufferCreateInfo buffer_desc = {
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = vertices_size,
            .props = 0,
        };
        model->vert_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
        Assert(model->vert_buf); // @todo report err
        SDL_SetGPUBufferName(APP.gpu.device, model->vert_buf, vrt_buf_name);
    }
    // create model index buffer
    {
        SDL_GPUBufferCreateInfo buffer_desc = {
            .usage = SDL_GPU_BUFFERUSAGE_INDEX,
            .size = indices_size,
            .props = 0,
        };
        model->ind_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
        Assert(model->ind_buf); // @todo report err
        SDL_SetGPUBufferName(APP.gpu.device, model->ind_buf, ind_buf_name);
    }
    // create model per instance storage buffer
    {
        SDL_GPUBufferCreateInfo buffer_desc = {
            .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            .size = sizeof(APP.rdr.models[0].data),
            .props = 0,
        };
        model->inst_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
        Assert(model->inst_buf); // @todo report err
        SDL_SetGPUBufferName(APP.gpu.device, model->inst_buf, inst_buf_name);
    }

    Gpu_TransferBuffer(model->vert_buf, vertices, vertices_size);
    Gpu_TransferBuffer(model->ind_buf, indices, indices_size);
}

static void Gpu_Init()
{
    APP.gpu.sample_count = SDL_GPU_SAMPLECOUNT_1;
    bool msaa = false;
    if (msaa)
    {
        SDL_GPUTextureFormat tex_format =
            SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window);

        bool supports_msaa =
            SDL_GPUTextureSupportsSampleCount(APP.gpu.device, tex_format,
                                              SDL_GPU_SAMPLECOUNT_4);

        if (supports_msaa)
        {
            APP.gpu.sample_count = SDL_GPU_SAMPLECOUNT_4;
        }
    }

    SDL_GPUColorTargetDescription color_desc = {
        .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
    };

    //
    // model
    //
    {
        ForU32(model_type, RdrModel_COUNT)
        {
            Gpu_InitModelBuffers(model_type);
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
                .code = g_ShaderModelVS,
                .code_size = sizeof(g_ShaderModelVS),
                .entrypoint = "ShaderGameVS",
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
                .num_uniform_buffers = 0,

                .format = SDL_GPU_SHADERFORMAT_DXIL,
                .code = g_ShaderModelPS,
                .code_size = sizeof(g_ShaderModelPS),
                .entrypoint = "ShaderGamePS",
            };
            fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
        }

        SDL_GPUGraphicsPipelineCreateInfo pipeline =
            Gpu_DefaultSDLPipeline(&color_desc, vertex_shader, fragment_shader);

        pipeline.vertex_input_state = (SDL_GPUVertexInputState)
        {
            .num_vertex_buffers = 2,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {
                {
                    .slot = 0,
                    .pitch = sizeof(Rdr_ModelVertex),
                    .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                    .instance_step_rate = 0,
                },
                {
                    .slot = 1,
                    .pitch = sizeof(Rdr_ModelInstanceData),
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
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                    .location = 2,
                    .offset = sizeof(float) * 6,
                },
            },
        };

        APP.gpu.model_pipeline = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
        Assert(APP.gpu.model_pipeline); // @todo report user to error and exit program on fail

        SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
        SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
    }

    //
    // wall
    //
    {
        // create wall vertex buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                .size = sizeof(APP.rdr.wall_verts),
                .props = 0,
            };
            APP.gpu.wall_vert_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.wall_vert_buf); // @todo report err
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.wall_vert_buf, "Wall vertex buffer");
        }

        // wall texture
        {
            SDL_Surface *imgs[WorldDir_COUNT] =
            {
                IMG_Load("../res/tex/brick_e.png"),
                IMG_Load("../res/tex/brick_w.png"),
                IMG_Load("../res/tex/brick_n.png"),
                IMG_Load("../res/tex/brick_s.png"),
                IMG_Load("../res/tex/brick_t.png"),
                IMG_Load("../res/tex/brick_b.png"),
            };
            ForArray(i, imgs)
            {
                Assert(imgs[i]); // @todo report err & use fallback textures
                Assert(imgs[0]->w == imgs[i]->w);
                Assert(imgs[0]->h == imgs[i]->h);
                Assert(imgs[0]->pitch == imgs[i]->pitch);
            }
            Assert(imgs[0]->pitch == imgs[0]->w*3); // rgb format expected

            SDL_GPUTextureCreateInfo tex_info =
            {
                .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                .width = imgs[0]->w,
                .height = imgs[0]->h,
                .layer_count_or_depth = ArrayCount(imgs),
                .num_levels = 1,
                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
            };
            APP.gpu.tex_wall = SDL_CreateGPUTexture(APP.gpu.device, &tex_info);
            SDL_SetGPUTextureName(APP.gpu.device, APP.gpu.tex_wall, "Tex Wall");

            U64 img_size = imgs[0]->w * imgs[0]->h * 4;
            ForArray(i, imgs)
            {
                ArenaScope scope = Arena_PushScope(APP.tmp);

                // temporary: convert rgb -> rgba
                U8 *buffer = Alloc(APP.tmp, U8, img_size);
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
                Gpu_TransferTexture(APP.gpu.tex_wall,
                                    i, imgs[i]->w, imgs[i]->h,
                                    buffer, img_size);

                Arena_PopScope(scope);
            }

            ForArray(i, imgs)
            {
                SDL_DestroySurface(imgs[i]);
            }
        }

        // wall sampler
        {
            SDL_GPUSamplerCreateInfo sampler_info =
            {
                .min_filter = SDL_GPU_FILTER_LINEAR,
                .mag_filter = SDL_GPU_FILTER_LINEAR,
                .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
                .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
                .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
                .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
            };
            APP.gpu.wall_sampler = SDL_CreateGPUSampler(APP.gpu.device, &sampler_info);
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
                .code = g_ShaderWallVS,
                .code_size = sizeof(g_ShaderWallVS),
                .entrypoint = "ShaderGameVS",
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
                .code = g_ShaderWallPS,
                .code_size = sizeof(g_ShaderWallPS),
                .entrypoint = "ShaderGamePS",
            };
            fragment_shader = SDL_CreateGPUShader(APP.gpu.device, &create_info);
        }

        SDL_GPUGraphicsPipelineCreateInfo pipeline =
            Gpu_DefaultSDLPipeline(&color_desc, vertex_shader, fragment_shader);

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
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
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

static void Gpu_Iterate()
{
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
    Assert(cmd); // @todo report err

    SDL_GPUTexture *swapchain_tex;
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
    ForArray(model_i, APP.gpu.models)
    {
        Gpu_ModelBuffers *gpu_model = APP.gpu.models + model_i;
        Rdr_Model *rdr_model = APP.rdr.models + model_i;
        if (rdr_model->count)
        {
            Gpu_TransferBuffer(gpu_model->inst_buf,
                               rdr_model->data,
                               rdr_model->count * sizeof(rdr_model->data[0]));
        }
    }

    // transform uniforms
    {
        SDL_PushGPUVertexUniformData(cmd, 0, &APP.camera_all_mat, sizeof(APP.camera_all_mat));
    }

    SDL_GPUDepthStencilTargetInfo depth_target = {
        .clear_depth = GPU_DEPTH_CLEAR_FLOAT,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .texture = APP.gpu.win_state.tex_depth,
        .cycle = true,
    };

    SDL_GPUColorTargetInfo color_target = {
        .clear_color =
        {
            .r = 0.6f,
            .g = 0.6f,
            .b = 0.8f,
            .a = 1.0f,
        },
    };
    bool tex_msaa = false;
    if (tex_msaa)
    {
        color_target.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op = SDL_GPU_STOREOP_RESOLVE;
        color_target.texture = APP.gpu.win_state.tex_msaa;
        color_target.resolve_texture = APP.gpu.win_state.tex_resolve;
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
            {
                SDL_GPUBufferBinding binding_vrt =
                {
                    .buffer = APP.gpu.wall_vert_buf,
                    .offset = 0,
                };
                SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);
            }
            // bind fragment sampler
            {
                SDL_GPUTextureSamplerBinding binding_sampl =
                {
                    .texture = APP.gpu.tex_wall,
                    .sampler = APP.gpu.wall_sampler,
                };
                SDL_BindGPUFragmentSamplers(pass, 0, &binding_sampl, 1);
            }

            SDL_DrawGPUPrimitives(pass, APP.rdr.wall_vert_count, 1, 0, 0);
        }

        // model
        SDL_BindGPUGraphicsPipeline(pass, APP.gpu.model_pipeline);
        ForArray(model_i, APP.gpu.models)
        {
            Gpu_ModelBuffers *gpu_model = APP.gpu.models + model_i;
            Rdr_Model *rdr_model = APP.rdr.models + model_i;

            if (!rdr_model->count)
                continue;

            // bind vertex buffer
            {
                SDL_GPUBufferBinding binding_vrt = {
                    .buffer = gpu_model->vert_buf,
                    .offset = 0,
                };
                SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);
            }
            // bind index buffer
            {
                SDL_GPUBufferBinding binding_ind = {
                    .buffer = gpu_model->ind_buf,
                    .offset = 0,
                };
                SDL_BindGPUIndexBuffer(pass, &binding_ind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
            }
            // bind instance storage buffer
            {
                SDL_BindGPUVertexStorageBuffers(pass, 0, &gpu_model->inst_buf, 1);
            }

            SDL_DrawGPUIndexedPrimitives(pass, gpu_model->ind_count, rdr_model->count, 0, 0, 0);
        }

        SDL_EndGPURenderPass(pass);
    }

#if 0 // @todo msaa
    if (render_state.sample_count > SDL_GPU_SAMPLECOUNT_1)
    {
        SDL_zero(blit_info);
        blit_info.source.texture = win_state->tex_resolve;
        blit_info.source.w = drawablew;
        blit_info.source.h = drawableh;

        blit_info.destination.texture = swapchain_tex;
        blit_info.destination.w = drawablew;
        blit_info.destination.h = drawableh;

        blit_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit_info.filter = SDL_GPU_FILTER_LINEAR;

        SDL_BlitGPUTexture(cmd, &blit_info);
    }
#endif

    SDL_SubmitGPUCommandBuffer(cmd);
}
