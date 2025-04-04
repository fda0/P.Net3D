static void AST_Init()
{
  // Init fallback texture
  {
    I32 dim = 512;
    I32 tex_size = dim*dim*sizeof(U32);
    U32 and_mask = (1 << 6);

    SDL_GPUTextureCreateInfo tex_info =
    {
      .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .width = dim,
      .height = dim,
      .layer_count_or_depth = 1,
      .num_levels = CalculateMipMapCount(dim, dim),
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
    };
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(APP.gpu.device, &tex_info);
    SDL_SetGPUTextureName(APP.gpu.device, texture, "Fallback texture");
    APP.ast.tex_fallback = texture;

    // Fill texture
    {
      SDL_GPUTransferBufferCreateInfo trans_desc =
      {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = tex_size
      };
      SDL_GPUTransferBuffer *trans_buf = SDL_CreateGPUTransferBuffer(APP.gpu.device, &trans_desc);
      Assert(trans_buf);

      // Fill GPU memory
      {
        U32 *map = SDL_MapGPUTransferBuffer(APP.gpu.device, trans_buf, false);
        U32 *pixel = map;
        ForI32(y, dim)
        {
          ForI32(x, dim)
          {
            pixel[0] = (x & y & and_mask) ? 0xff000000 : 0xffFF00FF;
            pixel += 1;
          }
        }
        SDL_UnmapGPUTransferBuffer(APP.gpu.device, trans_buf);
      }

      // GPU memory -> GPU buffers
      {
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTextureTransferInfo trans_info = { .transfer_buffer = trans_buf };
        SDL_GPUTextureRegion dst_region =
        {
          .texture = texture,
          .w = dim,
          .h = dim,
          .d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, &trans_info, &dst_region, false);

        SDL_EndGPUCopyPass(copy_pass);
        SDL_SubmitGPUCommandBuffer(cmd);
      }
      SDL_ReleaseGPUTransferBuffer(APP.gpu.device, trans_buf);
    }

    // Generate texture mipmap
    {
      SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
      SDL_GenerateMipmapsForGPUTexture(cmd, texture);
      SDL_SubmitGPUCommandBuffer(cmd);
    }
  }

  ForArray(i, APP.ast.tex_assets)
    APP.ast.tex_assets[i].texture = APP.ast.tex_fallback;
}

static void AST_Deinit()
{
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.tex_fallback);
  ForArray(i, APP.ast.tex_assets)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.tex_assets[i].texture);
}

static void AST_PostFrame()
{
  // Wake up texture asset loading thread
  if (APP.ast.tex_load_needed)
  {
    SDL_SignalSemaphore(APP.ast.tex_sem);
    APP.ast.tex_load_needed = false;
  }

  // Increment loaded_t
  ForArray(i, APP.ast.tex_assets)
  {
    Asset *asset = APP.ast.tex_assets + i;
    float speed = 10.f;
    asset->loaded_t += (float)asset->loaded * APP.dt * speed;
    asset->loaded_t = Min(1.f, asset->loaded_t);
  }
}

static Asset *TEX_GetAsset(TEX_Kind tex_kind)
{
  Assert(tex_kind < TEX_COUNT);

  Asset *result = APP.ast.tex_assets + tex_kind;
  result->last_touched_frame = APP.frame_id;

  if (!result->loaded)
    APP.ast.tex_load_needed = true;

  return result;
}

static void TEX_PrefetchAsset(TEX_Kind tex_kind)
{
  TEX_GetAsset(tex_kind);
}

//
// Thread
//
static void TEX_LoadTexture(TEX_Kind tex_kind, U64 min_frame)
{
  Assert(tex_kind < TEX_COUNT);
  Asset *asset = APP.ast.tex_assets + tex_kind;

  if (asset->loaded || asset->last_touched_frame < min_frame)
    return;

  S8 tex_name = TEX_GetName(tex_kind);

  struct
  {
    S8 postfix;
    U8 path_buffer[1024];
    SDL_Surface *surface;
  } params[] =
  {
    {S8Lit("Color.jpg")},
    {S8Lit("NormalGL.jpg")},
    {S8Lit("Roughness.jpg")},
    {S8Lit("Displacement.jpg")},
  };

  ForArray(i, params)
  {
    Printer p = Pr_Make(params[i].path_buffer, sizeof(params[i].path_buffer));
    Pr_S8(&p, S8Lit("../res/tex/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("_1K-JPG_"));
    Pr_S8(&p, params[i].postfix);

    const char *path = Pr_AsCstr(&p);
    params[i].surface = IMG_Load(path);
  }

  bool file_errors = false;
  ForArray(i, params)
  {
    if (!params[i].surface)
      file_errors = true;
  }

  I32 width = params[0].surface->w;
  I32 height = params[0].surface->h;

  if (!file_errors)
  {
    ForArray(i, params)
    {
      if (params[i].surface->w != width ||
          params[i].surface->h != height)
      {
        file_errors = true;
      }
    }
  }

  if (file_errors)
  {
    ForArray(i, params)
    {
      SDL_Surface *surface = params[i].surface;
      if (surface->format != SDL_PIXELFORMAT_RGB24)
      {
        params[i].surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
        SDL_DestroySurface(surface);

        if (!params[i].surface)
          file_errors = true;
      }
    }
  }

  if (file_errors)
  {
    ForArray(i, params)
      SDL_DestroySurface(params[i].surface);

    asset->error = true;
    asset->loaded = true;
    return;
  }

  I32 layer_count = ArrayCount(params);
  I32 gpu_layer_size = width * height * 4;
  I32 gpu_total_size = gpu_layer_size * layer_count;

  SDL_GPUTexture *texture = 0;

  // GPU transfer
  {
    SDL_GPUTransferBufferCreateInfo trans_buf_create_info =
    {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = gpu_total_size
    };
    SDL_GPUTransferBuffer *trans_buf = SDL_CreateGPUTransferBuffer(APP.gpu.device, &trans_buf_create_info);

    // Map CPU->GPU memory, copy surface pixels into GPU
    {
      U8 *gpu_mapped = SDL_MapGPUTransferBuffer(APP.gpu.device, trans_buf, false);
      U8 *gpu_pixel = gpu_mapped;

      ForArray(i, params)
      {
        U8 *surf_row = params[i].surface->pixels;
        I32 surf_pitch = params[i].surface->pitch;

        ForI32(y, height)
        {
          U8 *surf_pixel = surf_row;
          ForI32(x, width)
          {
            gpu_pixel[0] = surf_pixel[0];
            gpu_pixel[1] = surf_pixel[1];
            gpu_pixel[2] = surf_pixel[2];
            gpu_pixel[3] = 0xff;
            gpu_pixel += 4;
            surf_pixel += 3;
          }
          surf_row += surf_pitch;
        }
      }
      SDL_UnmapGPUTransferBuffer(APP.gpu.device, trans_buf);
    }

    // Create texture
    {
      SDL_GPUTextureCreateInfo texture_info =
      {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = width,
        .height = height,
        .layer_count_or_depth = layer_count,
        .num_levels = CalculateMipMapCount(width, height),
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
      };
      texture = SDL_CreateGPUTexture(APP.gpu.device, &texture_info);
    }

    // GPU memory -> GPU texture
    {
      SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);

      SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
      ForArray(i, params)
      {
        SDL_GPUTextureTransferInfo trans_info =
        {
          .transfer_buffer = trans_buf,
          .offset = i * gpu_layer_size,
        };
        SDL_GPUTextureRegion dst_region =
        {
          .texture = texture,
          .layer = i,
          .w = width,
          .h = height,
          .d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, &trans_info, &dst_region, false);
      }
      SDL_EndGPUCopyPass(copy_pass);

      SDL_GenerateMipmapsForGPUTexture(cmd, texture);
      SDL_SubmitGPUCommandBuffer(cmd);
    }

    SDL_ReleaseGPUTransferBuffer(APP.gpu.device, trans_buf);
  }

  asset->texture = texture;
  asset->loaded = true;
}

static int SDLCALL TEX_ThreadEntry(void *data)
{
  (void)data;

  for (;;)
  {
    U64 frame_margin = 2;
    U64 min_frame = APP.frame_id;
    if (min_frame >= frame_margin) min_frame -= frame_margin;
    else                           min_frame = 0;

    ForU32(tex_kind, TEX_COUNT)
      TEX_LoadTexture(tex_kind, min_frame);

    SDL_WaitSemaphore(APP.ast.tex_sem);
    if (APP.in_shutdown)
      return 0;
  }
}

static void TEX_InitThread()
{
  APP.ast.tex_sem = SDL_CreateSemaphore(0);
  SDL_Thread *asset_tex_thread = SDL_CreateThread(TEX_ThreadEntry, "Tex asset load thread", 0);
  SDL_DetachThread(asset_tex_thread);
}
