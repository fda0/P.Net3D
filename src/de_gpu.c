static void Gpu_LoadShader(bool is_vertex)
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
    }
}
