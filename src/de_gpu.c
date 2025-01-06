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
    else if (format & SDL_GPU_SHADERFORMAT_DXBC)
    {
        createinfo.format = SDL_GPU_SHADERFORMAT_DXBC;
        createinfo.code = is_vertex ? D3D11_CubeVert : D3D11_CubeFrag;
        createinfo.code_size = is_vertex ? SDL_arraysize(D3D11_CubeVert) : SDL_arraysize(D3D11_CubeFrag);
        createinfo.entrypoint = is_vertex ? "VSMain" : "PSMain";
    }
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

static void Gpu_Init()
{
    APP.gpu.vertex = Gpu_LoadShader(true);
    APP.gpu.fragment = Gpu_LoadShader(false);

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

}