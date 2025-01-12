static SDL_GPUShader *Gpu_LoadShader(bool is_vertex)
{
    SDL_GPUShaderCreateInfo createinfo;
    createinfo.num_samplers = 0;
    createinfo.num_storage_buffers = 0;
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
    SDL_GPUTextureCreateInfo createinfo = {};
    createinfo.type = SDL_GPU_TEXTURETYPE_2D;
    createinfo.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    createinfo.width = width;
    createinfo.height = height;
    createinfo.layer_count_or_depth = 1;
    createinfo.num_levels = 1;
    createinfo.sample_count = APP.gpu.sample_count;
    createinfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    createinfo.props = 0;

    SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);
    Assert(result); // @todo report err
    return result;
}

static SDL_GPUTexture *Gpu_CreateMSAATexture(Uint32 width, Uint32 height)
{
    if (APP.gpu.sample_count == SDL_GPU_SAMPLECOUNT_1)
    {
        return 0;
    }

    SDL_GPUTextureCreateInfo createinfo = {};
    createinfo.type = SDL_GPU_TEXTURETYPE_2D;
    createinfo.format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window);
    createinfo.width = width;
    createinfo.height = height;
    createinfo.layer_count_or_depth = 1;
    createinfo.num_levels = 1;
    createinfo.sample_count = APP.gpu.sample_count;
    createinfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    createinfo.props = 0;

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

    SDL_GPUTextureCreateInfo createinfo = {};
    createinfo.type = SDL_GPU_TEXTURETYPE_2D;
    createinfo.format = SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window);
    createinfo.width = width;
    createinfo.height = height;
    createinfo.layer_count_or_depth = 1;
    createinfo.num_levels = 1;
    createinfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createinfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    createinfo.props = 0;

    SDL_GPUTexture *result = SDL_CreateGPUTexture(APP.gpu.device, &createinfo);
    Assert(result); // @todo
    return result;
}

static void Gpu_ProcessWindowResize()
{
    U32 draw_width, draw_height;
    SDL_GetWindowSizeInPixels(APP.window, (int *)&draw_height, (int *)&draw_width);

    if (APP.gpu.window_state.draw_width != draw_width ||
        APP.gpu.window_state.draw_height != draw_height)
    {
        if (APP.gpu.window_state.tex_depth)
            SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.window_state.tex_depth);
        if (APP.gpu.window_state.tex_msaa)
            SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.window_state.tex_msaa);
        if (APP.gpu.window_state.tex_resolve)
            SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.window_state.tex_resolve);

        APP.gpu.window_state.tex_depth = Gpu_CreateDepthTexture(draw_width, draw_height);
        APP.gpu.window_state.tex_msaa = Gpu_CreateMSAATexture(draw_width, draw_height);
        APP.gpu.window_state.tex_resolve = Gpu_CreateResolveTexture(draw_width, draw_height);
    }

    APP.gpu.window_state.draw_width = draw_width;
    APP.gpu.window_state.draw_height = draw_height;
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
                    .num_vertex_buffers = 1,
                    .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
                    {
                        {
                            .slot = 0,
                            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                            .instance_step_rate = 0,
                            .pitch = sizeof(Rdr_Vertex),
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
        color_target.texture = APP.gpu.window_state.tex_msaa;
        color_target.resolve_texture = APP.gpu.window_state.tex_resolve;
        color_target.cycle = true;
        color_target.cycle_resolve_texture = true;
    }
    else
    {
        color_target.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op = SDL_GPU_STOREOP_STORE;
        color_target.texture = swapchain_tex;
    }

    SDL_GPUDepthStencilTargetInfo depth_target = {
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .texture = APP.gpu.window_state.tex_depth,
        .cycle = true,
    };


    float matrix_final[16];
    {
        float matrix_rotate[16], matrix_modelview[16], matrix_perspective[16];
        rotate_matrix((float)APP.gpu.window_state.angle_x, 1.0f, 0.0f, 0.0f, matrix_modelview);
        rotate_matrix((float)APP.gpu.window_state.angle_y, 0.0f, 1.0f, 0.0f, matrix_rotate);

        multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

        rotate_matrix((float)APP.gpu.window_state.angle_z, 0.0f, 1.0f, 0.0f, matrix_rotate);

        multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

        /* Pull the camera back from the cube */
        matrix_modelview[14] -= 2.5f * 10.f;

        perspective_matrix(45.0f, (float)draw_width/draw_height, 0.01f, 100.0f, matrix_perspective);
        multiply_matrix(matrix_perspective, matrix_modelview, (float*) &matrix_final);

        APP.gpu.window_state.angle_x += 0.3f;
        APP.gpu.window_state.angle_y += 0.2f;
        APP.gpu.window_state.angle_z += 0.1f;

        if(APP.gpu.window_state.angle_x >= 360) APP.gpu.window_state.angle_x -= 360;
        if(APP.gpu.window_state.angle_x < 0) APP.gpu.window_state.angle_x += 360;
        if(APP.gpu.window_state.angle_y >= 360) APP.gpu.window_state.angle_y -= 360;
        if(APP.gpu.window_state.angle_y < 0) APP.gpu.window_state.angle_y += 360;
        if(APP.gpu.window_state.angle_z >= 360) APP.gpu.window_state.angle_z -= 360;
        if(APP.gpu.window_state.angle_z < 0) APP.gpu.window_state.angle_z += 360;

    }
    SDL_PushGPUVertexUniformData(cmd, 0, matrix_final, sizeof(matrix_final));

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
    SDL_BindGPUGraphicsPipeline(pass, APP.gpu.pipeline);
    {
        SDL_GPUBufferBinding binding_vrt = {
            .buffer = APP.gpu.buf_vrt,
            .offset = 0,
        };
        SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);
    }
    {
        SDL_GPUBufferBinding binding_ind = {
            .buffer = APP.gpu.buf_ind,
            .offset = 0,
        };
        SDL_BindGPUIndexBuffer(pass, &binding_ind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    }
    SDL_DrawGPUIndexedPrimitives(pass, ArrayCount(MODEL_INDEX_ARR), 1, 0, 0, 0);
    //SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);

#if 0
    if (render_state.sample_count > SDL_GPU_SAMPLECOUNT_1)
    {
        SDL_zero(blit_info);
        blit_info.source.texture = winstate->tex_resolve;
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
