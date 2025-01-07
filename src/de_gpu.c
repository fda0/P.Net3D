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
#if 0
    else if (format & SDL_GPU_SHADERFORMAT_METALLIB)
    {
        createinfo.format = SDL_GPU_SHADERFORMAT_METALLIB;
        createinfo.code = is_vertex ? cube_vert_metallib : cube_frag_metallib;
        createinfo.code_size = is_vertex ? cube_vert_metallib_len : cube_frag_metallib_len;
        createinfo.entrypoint = is_vertex ? "vs_main" : "fs_main";
    }
    else if (...)
    {
        createinfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
        createinfo.code = is_vertex ? cube_vert_spv : cube_frag_spv;
        createinfo.code_size = is_vertex ? cube_vert_spv_len : cube_frag_spv_len;
        createinfo.entrypoint = "main";
    }
#endif
    else
    {
        Assert(0);
        // @todo report that user's gpu doesn't support anything we support
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

static void Gpu_Init()
{
    {
        SDL_GPUShader *vertex_shader = Gpu_LoadShader(true);
        SDL_GPUShader *fragment_shader = Gpu_LoadShader(false);

        // create vertex buffer
        {
            SDL_GPUBufferCreateInfo buffer_desc = {0};
            buffer_desc.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            buffer_desc.size = sizeof(g_vertex_data);
            buffer_desc.props = 0;
            APP.gpu.buf_vertex = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_desc);
            Assert(APP.gpu.buf_vertex); // @todo report to user if this failed and exit program
        }
        SDL_SetGPUBufferName(APP.gpu.device, APP.gpu.buf_vertex, "Vertex Buffer");

        // transfer buffer stuff
        {
            SDL_GPUTransferBuffer *buf_transfer = 0;
            // create transfer buffer
            {
                SDL_GPUTransferBufferCreateInfo transfer_buffer_desc;
                transfer_buffer_desc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                transfer_buffer_desc.size = sizeof(g_vertex_data);
                transfer_buffer_desc.props = 0;
                buf_transfer = SDL_CreateGPUTransferBuffer(APP.gpu.device, &transfer_buffer_desc);
                Assert(buf_transfer); // @todo report to user if this failed and exit program
            }

            // CPU -> GPU memory transfer
            {
                void *map = SDL_MapGPUTransferBuffer(APP.gpu.device, buf_transfer, false);
                memcpy(map, g_vertex_data, sizeof(g_vertex_data));
                SDL_UnmapGPUTransferBuffer(APP.gpu.device, buf_transfer);
            }

            {
                SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
                SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

                SDL_GPUTransferBufferLocation buf_location = {0};
                buf_location.transfer_buffer = buf_transfer;
                buf_location.offset = 0;

                SDL_GPUBufferRegion dst_region = {0};
                dst_region.buffer = APP.gpu.buf_vertex;
                dst_region.offset = 0;
                dst_region.size = sizeof(g_vertex_data);

                SDL_UploadToGPUBuffer(copy_pass, &buf_location, &dst_region, false);
                SDL_EndGPUCopyPass(copy_pass);
                SDL_SubmitGPUCommandBuffer(cmd);
            }

            SDL_ReleaseGPUTransferBuffer(APP.gpu.device, buf_transfer);
        }

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
            SDL_GPUColorTargetDescription color_target_desc = {};
            color_target_desc.format =
                SDL_GetGPUSwapchainTextureFormat(APP.gpu.device, APP.window);

            SDL_GPUVertexBufferDescription vertex_buffer_desc = {};
            vertex_buffer_desc.slot = 0;
            vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
            vertex_buffer_desc.instance_step_rate = 0;
            vertex_buffer_desc.pitch = sizeof(VertexData);

            SDL_GPUVertexAttribute vertex_attributes[2] = {};
            vertex_attributes[0].buffer_slot = 0;
            vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
            vertex_attributes[0].location = 0;
            vertex_attributes[0].offset = 0;

            vertex_attributes[1].buffer_slot = 0;
            vertex_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
            vertex_attributes[1].location = 1;
            vertex_attributes[1].offset = sizeof(float) * 3;

            SDL_GPUGraphicsPipelineCreateInfo pipelinedesc = {};
            pipelinedesc.target_info.num_color_targets = 1;
            pipelinedesc.target_info.color_target_descriptions = &color_target_desc;
            pipelinedesc.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            pipelinedesc.target_info.has_depth_stencil_target = true;

            pipelinedesc.depth_stencil_state.enable_depth_test = true;
            pipelinedesc.depth_stencil_state.enable_depth_write = true;
            pipelinedesc.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

            pipelinedesc.multisample_state.sample_count = APP.gpu.sample_count;

            pipelinedesc.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

            pipelinedesc.vertex_shader = vertex_shader;
            pipelinedesc.fragment_shader = fragment_shader;

            pipelinedesc.vertex_input_state.num_vertex_buffers = 1;
            pipelinedesc.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_desc;
            pipelinedesc.vertex_input_state.num_vertex_attributes = 2;
            pipelinedesc.vertex_input_state.vertex_attributes = (SDL_GPUVertexAttribute*) &vertex_attributes;

            pipelinedesc.props = 0;

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

    SDL_GPUTexture *swapchainTexture;
    U32 draw_width, draw_height;
    bool res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd, APP.window, &swapchainTexture, &draw_width, &draw_height);
    Assert(res); // @todo

    if (!swapchainTexture)
    {
        /* Swapchain is unavailable, cancel work */
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    Gpu_ProcessWindowResize();

    SDL_GPUColorTargetInfo color_target = {};
    color_target.clear_color.a = 1.0f;

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
        color_target.texture = swapchainTexture;
    }

    SDL_GPUDepthStencilTargetInfo depth_target = {};
    depth_target.clear_depth = 1.0f;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.texture = APP.gpu.window_state.tex_depth;
    depth_target.cycle = true;

    SDL_GPUBufferBinding vertex_binding = {};
    vertex_binding.buffer = APP.gpu.buf_vertex;
    vertex_binding.offset = 0;

    //SDL_PushGPUVertexUniformData(cmd, 0, matrix_final, sizeof(matrix_final));

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
    SDL_BindGPUGraphicsPipeline(pass, APP.gpu.pipeline);
    SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);

#if 0
    if (render_state.sample_count > SDL_GPU_SAMPLECOUNT_1)
    {
        SDL_zero(blit_info);
        blit_info.source.texture = winstate->tex_resolve;
        blit_info.source.w = drawablew;
        blit_info.source.h = drawableh;

        blit_info.destination.texture = swapchainTexture;
        blit_info.destination.w = drawablew;
        blit_info.destination.h = drawableh;

        blit_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit_info.filter = SDL_GPU_FILTER_LINEAR;

        SDL_BlitGPUTexture(cmd, &blit_info);
    }
#endif

    SDL_SubmitGPUCommandBuffer(cmd);
}
