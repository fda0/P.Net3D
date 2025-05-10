//
// Texture
//
static Asset *AST_GetTexture(TEX_Kind tex_kind)
{
  Assert(tex_kind < TEX_COUNT);

  Asset *result = APP.ast.tex_assets + tex_kind;
  result->last_touched_frame = APP.frame_id;

  if (!result->loaded)
    APP.ast.tex_load_needed = true;

  return result;
}

static void AST_PrefetchTexture(TEX_Kind tex_kind)
{
  AST_GetTexture(tex_kind);
}

static void AST_LoadTextureFromBreadFile(TEX_Kind tex_kind, U64 min_frame)
{
  Assert(tex_kind < TEX_COUNT);
  Asset *asset = APP.ast.tex_assets + tex_kind;

  if (asset->loaded || asset->last_touched_frame < min_frame)
    return;

  AST_BreadFile *br = &APP.ast.bread;
  Assert(tex_kind < (I32)br->materials_count);
  BREAD_Material *br_material = br->materials + tex_kind;
  BREAD_Texture *br_textures = BREAD_ListAsType(br_material->textures, BREAD_Texture);

  // Measure total gpu size for this material
  U32 gpu_total_size = 0;
  ForU32(tex_i, br_material->textures.count)
  {
    BREAD_Texture *br_tex = br_textures + tex_i;
    BREAD_TextureLod *br_lods = BREAD_ListAsType(br_tex->lods, BREAD_TextureLod);
    ForU32(lod_i, br_tex->lods.count)
    {
      BREAD_TextureLod *br_lod = br_lods + lod_i;
      gpu_total_size += br_lod->data.size;
    }
  }

  // Create texture and transfer CPU memory -> GPU memory -> GPU texture
  SDL_GPUTexture *texture = 0;
  {
    SDL_GPUTransferBufferCreateInfo transfer_create_info =
    {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = gpu_total_size
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(APP.gpu.device, &transfer_create_info);

    // CPU -> GPU
    {
      U8 *gpu_mapped = SDL_MapGPUTransferBuffer(APP.gpu.device, transfer, false);
      Memcpy(gpu_mapped, br_pixels, gpu_total_size);
      SDL_UnmapGPUTransferBuffer(APP.gpu.device, transfer);
    }

    // Create texture
    {
      SDL_GPUTextureCreateInfo texture_info =
      {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
         //.format = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB,
        .format = SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM,
        .width = br_material->width,
        .height = br_material->height,
        .layer_count_or_depth = br_material->layers,
        .num_levels = 1,
        //.num_levels = CalculateMipMapCount(br_material->width, br_material->height),
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      };
      texture = SDL_CreateGPUTexture(APP.gpu.device, &texture_info);
      SDL_SetGPUTextureName(APP.gpu.device, texture, "Texture from bread");
    }

    // GPU memory -> GPU texture
    {
      SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);

      SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
      ForU32(layer_index, br_material->layers)
      {
        SDL_GPUTextureTransferInfo source =
        {
          .transfer_buffer = transfer,
          .offset = layer_index * gpu_layer_size,
        };
        SDL_GPUTextureRegion destination =
        {
          .texture = texture,
          .layer = layer_index,
          .w = br_material->width,
          .h = br_material->height,
          .d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, &source, &destination, false);
      }
      SDL_EndGPUCopyPass(copy_pass);

      //SDL_GenerateMipmapsForGPUTexture(cmd, texture); // @todo CAN'T DO THAT WITH BC7 texture! Generate them offline?
      SDL_SubmitGPUCommandBuffer(cmd);
    }

    SDL_ReleaseGPUTransferBuffer(APP.gpu.device, transfer);
  }

  asset->Tex.handle = texture;
  asset->loaded = true;
}

static void AST_LoadTextureFromOnDiskFile(TEX_Kind tex_kind, U64 min_frame)
{
  if (tex_kind == TEX_Bricks071)
  {
    AST_LoadTextureFromBreadFile(tex_kind, min_frame);
    return;
  }

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
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
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

  asset->Tex.handle = texture;
  asset->loaded = true;
}

static int SDLCALL AST_TextureThread(void *data)
{
  (void)data;

  for (;;)
  {
    U64 frame_margin = 2;
    U64 min_frame = APP.frame_id;
    if (min_frame >= frame_margin) min_frame -= frame_margin;
    else                           min_frame = 0;

    ForU32(tex_kind, TEX_COUNT)
      AST_LoadTextureFromOnDiskFile(tex_kind, min_frame);

    SDL_WaitSemaphore(APP.ast.tex_sem);
    if (APP.in_shutdown)
      return 0;
  }
}

static void AST_InitTextureThread()
{
  APP.ast.tex_sem = SDL_CreateSemaphore(0);
  SDL_Thread *asset_tex_thread = SDL_CreateThread(AST_TextureThread, "Tex asset load thread", 0);
  SDL_DetachThread(asset_tex_thread);
}

//
// Skeleton
//
static Asset *AST_GetSkeleton(U32 skel_index)
{
  Assert(skel_index < APP.ast.skeletons_count);
  Asset *asset = APP.ast.skeletons + skel_index;
  asset->last_touched_frame = APP.frame_id;
  return asset;
}

static void AST_LoadSkeletons()
{
  AST_BreadFile *br = &APP.ast.bread;

  Assert(br->links->skeletons.elem_count <= ArrayCount(APP.ast.skeletons));
  U32 skeletons_count = Min(br->links->skeletons.elem_count, ArrayCount(APP.ast.skeletons));
  APP.ast.skeletons_count = skeletons_count;

  BREAD_Skeleton *skeletons = BREAD_FileRangeAsType(br->links->skeletons, BREAD_Skeleton);
  ForU32(skeleton_index, skeletons_count)
  {
    BREAD_Skeleton *br_skel = skeletons + skeleton_index;
    Asset *ast_skel = APP.ast.skeletons + skeleton_index;
    ast_skel->loaded = true;
    ast_skel->loaded_t = 1.f;
    ast_skel->Skel.root_transform = *BREAD_FileRangeAsType(br_skel->root_transform, Mat4);

    AN_Skeleton *skel = &ast_skel->Skel;
    skel->joints_count = br_skel->inv_bind_mats.elem_count;
    Assert(skel->joints_count == br_skel->inv_bind_mats.elem_count);
    Assert(skel->joints_count == br_skel->child_index_buf.elem_count);
    Assert(skel->joints_count == br_skel->child_index_ranges.elem_count);
    Assert(skel->joints_count == br_skel->translations.elem_count);
    Assert(skel->joints_count == br_skel->rotations.elem_count);
    Assert(skel->joints_count == br_skel->scales.elem_count);
    Assert(skel->joints_count == br_skel->name_ranges.elem_count);

    skel->inv_bind_mats      = BREAD_FileRangeAsType(br_skel->inv_bind_mats, Mat4);
    skel->child_index_buf    = BREAD_FileRangeAsType(br_skel->child_index_buf, U32);
    skel->child_index_ranges = BREAD_FileRangeAsType(br_skel->child_index_ranges, RngU32);
    skel->translations       = BREAD_FileRangeAsType(br_skel->translations, V3);
    skel->rotations          = BREAD_FileRangeAsType(br_skel->rotations, Quat);
    skel->scales             = BREAD_FileRangeAsType(br_skel->scales, V3);

    RngU32 *name_ranges = BREAD_FileRangeAsType(br_skel->name_ranges, RngU32);
    skel->names_s8 = Alloc(br->arena, S8, skel->joints_count);
    ForU32(i, skel->joints_count)
      skel->names_s8[i] = S8_Substring(BREAD_File(), name_ranges[i].min, name_ranges[i].max);

    // Animations
    skel->animations_count = br_skel->animations.elem_count;
    BREAD_Animation *br_animations = BREAD_FileRangeAsType(br_skel->animations, BREAD_Animation);
    skel->animations = Alloc(br->arena, AN_Animation, skel->animations_count);

    ForU32(animation_index, skel->animations_count)
    {
      AN_Animation *anim = skel->animations + animation_index;
      BREAD_Animation *br_anim = br_animations + animation_index;

      anim->name_s8 = S8_Make(BREAD_FileRangeAsType(br_anim->name, U8), br_anim->name.size);

      anim->t_min = br_anim->t_min;
      anim->t_max = br_anim->t_max;

      anim->channels_count = br_anim->channels.elem_count;
      BREAD_AnimChannel *br_channels = BREAD_FileRangeAsType(br_anim->channels, BREAD_AnimChannel);
      anim->channels = Alloc(br->arena, AN_Channel, anim->channels_count);

      ForU32(channel_index, anim->channels_count)
      {
        AN_Channel *chan = anim->channels + channel_index;
        BREAD_AnimChannel *br_chan = br_channels + channel_index;

        chan->joint_index = br_chan->joint_index;
        chan->type = br_chan->type;
        chan->count = br_chan->inputs.elem_count;

        U32 comp_count = (chan->type == AN_Rotation ? 4 : 3);
        Assert(comp_count * chan->count == br_chan->outputs.elem_count);

        chan->inputs = BREAD_FileRangeAsType(br_chan->inputs, float);
        chan->outputs = BREAD_FileRangeAsType(br_chan->outputs, float);
      }
    }
  }
}

//
// Geometry
//
static Asset *AST_GetGeometry(MODEL_Type model_type)
{
  // @todo @speed Stream geometry assets in the future.
  Assert(model_type < MODEL_COUNT);
  Asset *asset = APP.ast.geo_assets + model_type;
  asset->last_touched_frame = APP.frame_id;
  return asset;
}

static void AST_LoadGeometry()
{
  AST_BreadFile *br = &APP.ast.bread;

  APP.ast.rigid_vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                            br->links->models.rigid_vertices.size,
                                            "Rigid model vertices");
  APP.ast.skinned_vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                              br->links->models.skinned_vertices.size,
                                              "Skinned model vertices");
  APP.ast.indices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX,
                                     br->links->models.indices.size,
                                     "Model indices");

  S8 rigid_string = BREAD_FileRangeToS8(br->links->models.rigid_vertices);
  S8 skinned_string = BREAD_FileRangeToS8(br->links->models.skinned_vertices);
  S8 indices_string = BREAD_FileRangeToS8(br->links->models.indices);

  GPU_TransferBuffer(APP.ast.rigid_vertices, rigid_string.str, rigid_string.size);
  GPU_TransferBuffer(APP.ast.skinned_vertices, skinned_string.str, skinned_string.size);
  GPU_TransferBuffer(APP.ast.indices, indices_string.str, indices_string.size);

  ForU32(model_kind, MODEL_COUNT)
  {
    BREAD_Model *br_model = br->models + model_kind;
    Asset *asset = APP.ast.geo_assets + model_kind;

    asset->loaded = true;
    asset->Geo.is_skinned = br_model->is_skinned;
    asset->Geo.vertices_start_index = br_model->vertices_start_index;
    asset->Geo.indices_start_index = br_model->indices_start_index;
    asset->Geo.indices_count = br_model->indices_count;
    asset->Geo.skeleton_index = br_model->skeleton_index;
  }
}

//
//
//
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

static void AST_Init()
{
  BREAD_LoadFile("data.bread");
  AST_LoadSkeletons();
  AST_LoadGeometry();

  // Init textures, create fallback texture
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

    ForArray(i, APP.ast.tex_assets)
    {
      Asset *asset = APP.ast.tex_assets + i;
      asset->Tex.handle = APP.ast.tex_fallback;
      asset->Tex.shininess = 16.f;
    }
  }
}

static void AST_Deinit()
{
  // textures
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.tex_fallback);
  ForArray(i, APP.ast.tex_assets)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.tex_assets[i].Tex.handle);

  // geometry
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.rigid_vertices);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.skinned_vertices);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.indices);
}
