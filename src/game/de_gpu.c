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
        // create model vertex buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                .size = sizeof(MODEL_VERTEX_ARR),
                .props = 0,
            };
            APP.gpu.model_vert_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.model_vert_buf); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.model_vert_buf, "Model vertex buffer");
        }
        // create model index buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_INDEX,
                .size = sizeof(MODEL_VERTEX_ARR),
                .props = 0,
            };
            APP.gpu.model_indx_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.model_indx_buf); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.model_indx_buf, "Model index buffer");
        }
        // create model per instance storage buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                .size = sizeof(APP.rdr.instance_data),
                .props = 0,
            };
            APP.gpu.model_instance_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.model_instance_buf); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.model_instance_buf, "Per model instance storage buffer");
        }

        // model buffer transfers
        // @todo merge transfers into one mapping, two memcpy calls
        Gpu_TransferBuffer(APP.gpu.model_vert_buf, MODEL_VERTEX_ARR, sizeof(MODEL_VERTEX_ARR));
        Gpu_TransferBuffer(APP.gpu.model_indx_buf, MODEL_INDEX_ARR, sizeof(MODEL_INDEX_ARR));

        SDL_GPUShader *vertex_shader = Gpu_LoadShader(true);
        SDL_GPUShader *fragment_shader = Gpu_LoadShader(false);

        SDL_GPUGraphicsPipelineCreateInfo pipeline =
            Gpu_DefaultSDLPipeline(&color_desc, vertex_shader, fragment_shader);

        pipeline.vertex_input_state = (SDL_GPUVertexInputState)
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
            Assert(APP.gpu.wall_vert_buf); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.wall_vert_buf, "Wall vertex buffer");
        }
        // create wall index buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_INDEX,
                .size = sizeof(APP.rdr.wall_indices),
                .props = 0,
            };
            APP.gpu.wall_indx_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.wall_indx_buf); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.wall_indx_buf, "Wall index buffer");
        }
        // create wall per instance storage buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {
                .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                .size = sizeof(Rdr_ModelInstanceData),
                .props = 0,
            };
            APP.gpu.wall_instance_buf = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.wall_instance_buf); // @todo report
            SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.wall_instance_buf, "Per wall instance storage buffer");

            // transfer single instance data
            Rdr_ModelInstanceData noop_instance_data = {};
            noop_instance_data.transform = Mat4_Diagonal(1.f);
            noop_instance_data.color = (V4){1,1,1,1};
            Gpu_TransferBuffer(APP.gpu.wall_instance_buf, &noop_instance_data, sizeof(noop_instance_data));
        }

        // prepare wall indices
        {
            U16 indices[36] =
            {
                0,1,4, 1,4,5,
                0,1,2, 0,3,2,
                1,2,6, 1,6,5,
                4,5,7, 5,6,7,
                3,7,6, 2,3,6,
                0,4,7, 0,7,3,
            };

            U32 indices_per_cube = 36;
            ForU32(i, ArrayCount(APP.rdr.wall_indices) / indices_per_cube)
            {
                U16 *idx = APP.rdr.wall_indices + (i * indices_per_cube);
                U32 off = i * 8;
                ForArray(j, indices)
                {
                    idx[j] = off + indices[j];
                }
            }

            Gpu_TransferBuffer(APP.gpu.wall_indx_buf, APP.rdr.wall_indices, sizeof(APP.rdr.wall_indices));
        }

        SDL_GPUShader *vertex_shader = Gpu_LoadShader(true);
        SDL_GPUShader *fragment_shader = Gpu_LoadShader(false);

        SDL_GPUGraphicsPipelineCreateInfo pipeline =
            Gpu_DefaultSDLPipeline(&color_desc, vertex_shader, fragment_shader);

        pipeline.vertex_input_state = (SDL_GPUVertexInputState)
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

        APP.gpu.wall_pipeline = SDL_CreateGPUGraphicsPipeline(APP.gpu.device, &pipeline);
        Assert(APP.gpu.wall_pipeline); // @todo report user to error and exit program on fail

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
        Gpu_TransferBuffer(APP.gpu.model_instance_buf, APP.rdr.instance_data,
                           APP.rdr.instance_count*sizeof(APP.rdr.instance_data[0]));
    }

    if (APP.rdr.wall_vert_count)
    {
        Gpu_TransferBuffer(APP.gpu.wall_vert_buf, APP.rdr.wall_verts, sizeof(APP.rdr.wall_verts));
    }

    // camera matrices
    {
        Mat4 camera_move_mat = Mat4_InvTranslation(Mat4_Translation(APP.camera_p));

        Mat4 camera_rot_mat = Mat4_Rotation_RH(APP.camera_rot.x, (V3){1,0,0});
        camera_rot_mat = Mat4_Mul(Mat4_Rotation_RH(APP.camera_rot.y, (V3){0,1,0}), camera_rot_mat);
        camera_rot_mat = Mat4_Mul(Mat4_Rotation_RH(APP.camera_rot.z, (V3){0,0,1}), camera_rot_mat);

        Mat4 perspective_mat = Mat4_Perspective_RH_NO(0.21f, (float)draw_width/draw_height, 1.f, 1000.f);

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

        // model
        SDL_BindGPUGraphicsPipeline(pass, APP.gpu.model_pipeline);
        {
            // bind vertex buffer
            {
                SDL_GPUBufferBinding binding_vrt = {
                    .buffer = APP.gpu.model_vert_buf,
                    .offset = 0,
                };
                SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);
            }
            // bind index buffer
            {
                SDL_GPUBufferBinding binding_ind = {
                    .buffer = APP.gpu.model_indx_buf,
                    .offset = 0,
                };
                SDL_BindGPUIndexBuffer(pass, &binding_ind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
            }
            // bind instance storage buffer
            {
                SDL_BindGPUVertexStorageBuffers(pass, 0, &APP.gpu.model_instance_buf, 1);
            }

            SDL_DrawGPUIndexedPrimitives(pass, ArrayCount(MODEL_INDEX_ARR), APP.rdr.instance_count, 0, 0, 0);
        }

        // walls
        SDL_BindGPUGraphicsPipeline(pass, APP.gpu.wall_pipeline);
        {
            U32 index_count = (APP.rdr.wall_vert_count / 8) * 36;

            // bind vertex buffer
            {
                SDL_GPUBufferBinding binding_vrt = {
                    .buffer = APP.gpu.wall_vert_buf,
                    .offset = 0,
                };
                SDL_BindGPUVertexBuffers(pass, 0, &binding_vrt, 1);
            }
            // bind index buffer
            {
                SDL_GPUBufferBinding binding_ind = {
                    .buffer = APP.gpu.wall_indx_buf,
                    .offset = 0,
                };
                SDL_BindGPUIndexBuffer(pass, &binding_ind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
            }
            // bind instance storage buffer
            {
                SDL_BindGPUVertexStorageBuffers(pass, 0, &APP.gpu.wall_instance_buf, 1);
            }

            SDL_DrawGPUIndexedPrimitives(pass, index_count, 1, 0, 0, 0);
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
