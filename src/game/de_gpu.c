#define GPU_DEPTH_CLEAR_FLOAT 1.0f

static SDL_GPUShader *Gpu_LoadShader(bool is_vertex)
{
    SDL_GPUShaderCreateInfo createinfo;
    createinfo.num_samplers = 0;
    createinfo.num_storage_buffers = is_vertex ? 1 : 0;
    createinfo.num_storage_textures = 0;
    createinfo.num_uniform_buffers = is_vertex ? 1 : 0;
    createinfo.props = 0;

    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(APP.gpu.device);
    if (format & SDL_GPU_SHADERFORMAT_DXIL)
    {
        createinfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        createinfo.code = is_vertex ? g_ShaderGameVS : g_ShaderGamePS;
        createinfo.code_size = is_vertex ? sizeof(g_ShaderGameVS) : sizeof(g_ShaderGamePS);
        createinfo.entrypoint = is_vertex ? "ShaderGameVS" : "ShaderGamePS";
    }
    else
    {
        Assert(0); // @todo report that user's gpu doesn't support anything we support
        return 0;
    }

    createinfo.stage = is_vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    return SDL_CreateGPUShader(APP.gpu.device, &createinfo);
}

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

static SDL_GPUTexture *Gpu_CreateMSAATexture(Uint32 width, Uint32 height)
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
    Assert(result); // @todo
    return result;
}

static SDL_GPUTexture *Gpu_CreateResolveTexture(Uint32 width, Uint32 height)
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
    Assert(result); // @todo
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
    SDL_GPUTransferBuffer *buf_transfer = 0;
    {
        SDL_GPUTransferBufferCreateInfo transfer_buffer_desc;
        transfer_buffer_desc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_buffer_desc.size = data_size;
        transfer_buffer_desc.props = 0;
        buf_transfer = SDL_CreateGPUTransferBuffer(APP.gpu.device, &transfer_buffer_desc);
        Assert(buf_transfer); // @todo report
    }

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

static void Gpu_Init()
{
    {
        SDL_GPUShader *vertex_shader = Gpu_LoadShader(true);
        SDL_GPUShader *fragment_shader = Gpu_LoadShader(false);

        // create vertex buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                .size = sizeof(MODEL_VERTEX_ARR),
                .props = 0,
            };
            APP.gpu.buf_vrt = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.buf_vrt); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.buf_vrt, "Vertex Buffer");
        }

        // create index buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_INDEX,
                .size = sizeof(MODEL_VERTEX_ARR),
                .props = 0,
            };
            APP.gpu.buf_ind = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.buf_ind); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.buf_ind, "Index Buffer");
        }

        // create storage buffer (per model instance data)
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                .size = sizeof(APP.rdr.instance_data),
                .props = 0,
            };
            APP.gpu.buf_instance_storage = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.buf_instance_storage); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.buf_instance_storage, "Per model instance storage buffer");
        }

        // transfer buffer stuff;
        // @todo merge transfers into one mapping, two memcpy calls
        Gpu_TransferBuffer(APP.gpu.buf_vrt, MODEL_VERTEX_ARR, sizeof(MODEL_VERTEX_ARR));
        Gpu_TransferBuffer(APP.gpu.buf_ind, MODEL_INDEX_ARR, sizeof(MODEL_INDEX_ARR));

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

        // setup pipeline
        {
            SDL_GPUColorTargetDescription color_target_desc = {
                .format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window),
            };

            SDL_GPUGraphicsPipelineCreateInfo pipelinedesc =
            {
                .target_info =
                {
                    .num_color_targets = 1,
                    .color_target_descriptions = &color_target_desc,
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
                .vertex_input_state =
                {
                    .num_vertex_buffers = 2,
                    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
                    {
                        {
                            .slot = 0,
                            .pitch = sizeof(Rdr_Vertex),
                            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                            .instance_step_rate = 0,
                        },
#if 1
                        {
                            .slot = 1,
                            .pitch = sizeof(Rdr_ModelInstanceData),
                            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                            .instance_step_rate = 0,
                        },
#endif
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
                },
                .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                .vertex_shader = vertex_shader,
                .fragment_shader = fragment_shader,
                .props = 0
            };

            APP.gpu.pipeline = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipelinedesc);
            Assert(APP.gpu.pipeline); // @todo report user to error and exit program on fail
        }

        SDL_ReleaseGPUShader(APP.gpu.device, vertex_shader);
        SDL_ReleaseGPUShader(APP.gpu.device, fragment_shader);
    }

    Gpu_ProcessWindowResize();
}

static void Gpu_Iterate()
{
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
    Assert(cmd); // @todo

    SDL_GPUTexture *swapchain_tex;
    U32 draw_width, draw_height;
    bool res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd, APP.window, &swapchain_tex, &draw_width, &draw_height);
    Assert(res); // @todo

    if (!swapchain_tex)
    {
        /* Swapchain is unavailable, cancel work */
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    Gpu_ProcessWindowResize();

    // upload instance data
    if (APP.rdr.instance_count)
    {
        Assert(APP.rdr.instance_count <= ArrayCount(APP.rdr.instance_data));
        Gpu_TransferBuffer(APP.gpu.buf_instance_storage, APP.rdr.instance_data,
                           APP.rdr.instance_count*sizeof(APP.rdr.instance_data[0]));
    }

    {
        APP.gpu.win_state.camera_rot = (V3){0.1f, 0.0f, 0.0f};
        //APP.gpu.win_state.camera_rot.x += 0.0006f;
        //APP.gpu.win_state.camera_rot.y += 0.0004f;
        //APP.gpu.win_state.camera_rot.z += 0.0002f;
        //APP.gpu.win_state.camera_rot.x = WrapF(0.f, 1.f, APP.gpu.win_state.camera_rot.x);
        //APP.gpu.win_state.camera_rot.y = WrapF(0.f, 1.f, APP.gpu.win_state.camera_rot.y);
        //APP.gpu.win_state.camera_rot.z = WrapF(0.f, 1.f, APP.gpu.win_state.camera_rot.z);

        Mat4 camera_rot_mat = Mat4_Rotation_RH(APP.gpu.win_state.camera_rot.x, (V3){1,0,0});
        camera_rot_mat = Mat4_Mul(Mat4_Rotation_RH(APP.gpu.win_state.camera_rot.y, (V3){0,1,0}), camera_rot_mat);
        camera_rot_mat = Mat4_Mul(Mat4_Rotation_RH(APP.gpu.win_state.camera_rot.z, (V3){0,0,1}), camera_rot_mat);

        V3 camera_move = {};
        camera_move.z = -20.f;
        Mat4 camera_move_mat = Mat4_Translation(camera_move);

        // ----
        Mat4 perspective_mat = Mat4_Perspective_RH_NO(0.21f, (float)draw_width/draw_height, 0.01f, 1000.f);


        Mat4 all_mats[] =
        {
            camera_move_mat,
            camera_rot_mat,
            perspective_mat,
        };
        SDL_PushGPUVertexUniformData(cmd, 0, all_mats, sizeof(all_mats));
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
        SDL_BindGPUGraphicsPipeline(pass, APP.gpu.pipeline);

        // bind vertex buffer
        {
            SDL_GPUBufferBinding binding_vrt = {
                .buffer = APP.gpu.buf_vrt,
                .offset = 0,
            };
            SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);
        }

        // bind index buffer
        {
            SDL_GPUBufferBinding binding_ind = {
                .buffer = APP.gpu.buf_ind,
                .offset = 0,
            };
            SDL_BindGPUIndexBuffer(pass, &binding_ind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        }

        // bind instance storage buffer
        {
            SDL_BindGPUVertexStorageBuffers(pass, 0, &APP.gpu.buf_instance_storage, 1);
        }

        SDL_DrawGPUIndexedPrimitives(pass, ArrayCount(MODEL_INDEX_ARR), APP.rdr.instance_count, 0, 0, 0);
        //SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
    }

#if 0
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
