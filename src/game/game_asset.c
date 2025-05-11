//
// Texture
//
static ASSET_Texture *ASSET_GetTexture(TEX_Kind tex_kind)
{
  Assert(tex_kind < TEX_COUNT);

  ASSET_Texture *result = APP.ast.textures + tex_kind;
  result->b.last_touched_frame = APP.frame_id;

  if (!result->b.loaded)
    APP.ast.tex_load_needed = true;

  return result;
}

static void ASSET_PrefetchTexture(TEX_Kind tex_kind)
{
  ASSET_GetTexture(tex_kind);
}

static void ASSET_LoadTextureFromBreadFile(TEX_Kind tex_kind, U64 min_frame)
{
  Assert(tex_kind < TEX_COUNT);
  ASSET_Texture *asset = APP.ast.textures + tex_kind;

  if (asset->b.loaded || asset->b.last_touched_frame < min_frame)
    return;

  ASSET_BreadFile *br = &APP.ast.bread;

  if (tex_kind >= br->materials_count)
  {
    asset->b.error = true;
    asset->b.loaded = true;
    return;
  }

  // Material from .bread - it contains a big buffer that needs to uploaded to GPU
  BREAD_Material *br_material = br->materials + tex_kind;
  S8 gpu_full_data = BREAD_ListToS8(br_material->full_data);

  // MaterialSections - it contains a mapping from big buffer to individual pieces of texture layers and lods
  U32 br_sections_count = br_material->sections.count;
  BREAD_MaterialSection *br_sections = BREAD_ListAsType(br_material->sections, BREAD_MaterialSection);

  U32 lods_count = br_material->lods;
  bool generate_lods = false;
  if (lods_count == 1 && br_material->format != BREAD_Tex_BC7_RGBA)
  {
    lods_count = CalculateMipMapCount(br_material->width, br_material->height);
    generate_lods = true;
  }


  // Create texture and transfer CPU memory -> GPU memory -> GPU texture
  SDL_GPUTexture *texture = 0;
  {
    SDL_GPUTransferBufferCreateInfo transfer_create_info =
    {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = gpu_full_data.size * 4
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(APP.gpu.device, &transfer_create_info);

    // CPU -> GPU
    {
      U8 *gpu_mapped = SDL_MapGPUTransferBuffer(APP.gpu.device, transfer, false);
      Memcpy(gpu_mapped, gpu_full_data.str, gpu_full_data.size);
      SDL_UnmapGPUTransferBuffer(APP.gpu.device, transfer);
    }

    // Create texture
    {
      SDL_GPUTextureFormat sdl_format = (br_material->format == BREAD_Tex_R8G8B8A8 ?
                                         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM :
                                         SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM);

      SDL_GPUTextureCreateInfo texture_info =
      {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        .format = sdl_format,
        .width = br_material->width,
        .height = br_material->height,
        .layer_count_or_depth = br_material->layers,
        .num_levels = lods_count,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
      };

      if (generate_lods)
        texture_info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

      texture = SDL_CreateGPUTexture(APP.gpu.device, &texture_info);
      SDL_SetGPUTextureName(APP.gpu.device, texture, "Texture from bread");
    }

    // GPU memory -> GPU texture
    {
      SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
      SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

      ForU32(br_section_index, br_sections_count)
      {
        BREAD_MaterialSection *br_sect = br_sections + br_section_index;

        SDL_GPUTextureTransferInfo source =
        {
          .transfer_buffer = transfer,
          .offset = br_sect->data_offset,
          .pixels_per_row = br_sect->width,
        };
        SDL_GPUTextureRegion destination =
        {
          .texture = texture,
          .mip_level = br_sect->lod,
          .layer = br_sect->layer,
          .w = br_sect->width,
          .h = br_sect->height,
          .d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, &source, &destination, false);
      }

      SDL_EndGPUCopyPass(copy_pass);

      if (generate_lods)
        SDL_GenerateMipmapsForGPUTexture(cmd, texture);

      SDL_SubmitGPUCommandBuffer(cmd);
    }

    SDL_ReleaseGPUTransferBuffer(APP.gpu.device, transfer);
  }

  asset->handle = texture;
  asset->b.loaded = true;
}

static int SDLCALL ASSET_TextureThread(void *data)
{
  (void)data;

  for (;;)
  {
    U64 frame_margin = 2;
    U64 min_frame = APP.frame_id;
    if (min_frame >= frame_margin) min_frame -= frame_margin;
    else                           min_frame = 0;

    ForU32(tex_kind, TEX_COUNT)
      ASSET_LoadTextureFromBreadFile(tex_kind, min_frame);

    SDL_WaitSemaphore(APP.ast.tex_sem);
    if (APP.in_shutdown)
      return 0;
  }
}

static void ASSET_InitTextureThread()
{
  APP.ast.tex_sem = SDL_CreateSemaphore(0);
  SDL_Thread *asset_tex_thread = SDL_CreateThread(ASSET_TextureThread, "Tex asset load thread", 0);
  SDL_DetachThread(asset_tex_thread);
}

//
// Skeleton
//
static ASSET_Skeleton *ASSET_GetSkeleton(U32 skel_index)
{
  Assert(skel_index < APP.ast.skeletons_count);
  return APP.ast.skeletons + skel_index;
}

static void ASSET_LoadSkeletons()
{
  ASSET_BreadFile *br = &APP.ast.bread;

  U32 br_skeletons_count = br->links->skeletons.count;
  BREAD_Skeleton *br_skeletons = BREAD_ListAsType(br->links->skeletons, BREAD_Skeleton);

  U32 skeletons_count = Min(br_skeletons_count, ArrayCount(APP.ast.skeletons));
  APP.ast.skeletons_count = skeletons_count;

  ForU32(skeleton_index, skeletons_count)
  {
    BREAD_Skeleton *br_skel = br_skeletons + skeleton_index;
    ASSET_Skeleton *skel = APP.ast.skeletons + skeleton_index;
    skel->root_transform = br_skel->root_transform;

    skel->joints_count = br_skel->inv_bind_mats.count;
    Assert(skel->joints_count == br_skel->inv_bind_mats.count);
    Assert(skel->joints_count == br_skel->child_index_buf.count);
    Assert(skel->joints_count == br_skel->child_index_ranges.count);
    Assert(skel->joints_count == br_skel->translations.count);
    Assert(skel->joints_count == br_skel->rotations.count);
    Assert(skel->joints_count == br_skel->scales.count);
    Assert(skel->joints_count == br_skel->name_ranges.count);

    skel->inv_bind_mats      = BREAD_ListAsType(br_skel->inv_bind_mats, Mat4);
    skel->child_index_buf    = BREAD_ListAsType(br_skel->child_index_buf, U32);
    skel->child_index_ranges = BREAD_ListAsType(br_skel->child_index_ranges, RngU32);
    skel->translations       = BREAD_ListAsType(br_skel->translations, V3);
    skel->rotations          = BREAD_ListAsType(br_skel->rotations, Quat);
    skel->scales             = BREAD_ListAsType(br_skel->scales, V3);

    RngU32 *name_ranges = BREAD_ListAsType(br_skel->name_ranges, RngU32);
    skel->names_s8 = Alloc(br->arena, S8, skel->joints_count);
    ForU32(i, skel->joints_count)
      skel->names_s8[i] = S8_Substring(BREAD_File(), name_ranges[i].min, name_ranges[i].max);

    // Animations
    skel->anims_count = br_skel->anims.count;
    BREAD_Animation *br_anims = BREAD_ListAsType(br_skel->anims, BREAD_Animation);
    skel->anims = Alloc(br->arena, ASSET_Animation, skel->anims_count);

    ForU32(anim_index, skel->anims_count)
    {
      ASSET_Animation *anim = skel->anims + anim_index;
      BREAD_Animation *br_anim = br_anims + anim_index;

      anim->name_s8 = S8_Make(BREAD_ListAsType(br_anim->name, U8), br_anim->name.size);

      anim->t_min = br_anim->t_min;
      anim->t_max = br_anim->t_max;

      anim->channels_count = br_anim->channels.count;
      BREAD_AnimationChannel *br_channels = BREAD_ListAsType(br_anim->channels, BREAD_AnimationChannel);
      anim->channels = Alloc(br->arena, ASSET_AnimationChannel, anim->channels_count);

      ForU32(channel_index, anim->channels_count)
      {
        ASSET_AnimationChannel *chan = anim->channels + channel_index;
        BREAD_AnimationChannel *br_chan = br_channels + channel_index;

        chan->joint_index = br_chan->joint_index;
        chan->type = br_chan->type;
        chan->count = br_chan->inputs.count;

        U32 comp_count = (chan->type == AN_Rotation ? 4 : 3);
        Assert(comp_count * chan->count == br_chan->outputs.count);

        chan->inputs = BREAD_ListAsType(br_chan->inputs, float);
        chan->outputs = BREAD_ListAsType(br_chan->outputs, float);
      }
    }
  }
}

//
// Geometry
//
static ASSET_Model *ASSET_GetModel(MODEL_Type model_type)
{
  // @todo @speed Stream geometry assets in the future.
  Assert(model_type < MODEL_COUNT);
  return APP.ast.models + model_type;
}

static void ASSET_LoadGeometry()
{
  ASSET_BreadFile *br = &APP.ast.bread;

  APP.ast.rigid_vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                            br->links->models.rigid_vertices.size,
                                            "Rigid model vertices");
  APP.ast.skinned_vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                              br->links->models.skinned_vertices.size,
                                              "Skinned model vertices");
  APP.ast.indices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX,
                                     br->links->models.indices.size,
                                     "Model indices");

  S8 rigid_string = BREAD_ListToS8(br->links->models.rigid_vertices);
  S8 skinned_string = BREAD_ListToS8(br->links->models.skinned_vertices);
  S8 indices_string = BREAD_ListToS8(br->links->models.indices);

  GPU_TransferBuffer(APP.ast.rigid_vertices, rigid_string.str, rigid_string.size);
  GPU_TransferBuffer(APP.ast.skinned_vertices, skinned_string.str, skinned_string.size);
  GPU_TransferBuffer(APP.ast.indices, indices_string.str, indices_string.size);

  ForU32(model_kind, MODEL_COUNT)
  {
    BREAD_Model *br_model = br->models + model_kind;
    ASSET_Model *asset = APP.ast.models + model_kind;
    asset->is_skinned = br_model->is_skinned;
    asset->skeleton_index = br_model->skeleton_index;
    asset->geos_count = br_model->geometries.count;
    asset->geos = AllocZeroed(br->arena, ASSET_Geometry, asset->geos_count);

    BREAD_Geometry *br_geos = BREAD_ListAsType(br_model->geometries, BREAD_Geometry);

    ForU32(geo_index, asset->geos_count)
    {
      BREAD_Geometry *br_geo = br_geos + geo_index;
      ASSET_Geometry *geo = asset->geos + geo_index;
      geo->vertices_start_index = br_geo->vertices_start_index;
      geo->indices_start_index = br_geo->indices_start_index;
      geo->indices_count = br_geo->indices_count;
    }
  }
}

//
//
//
static void ASSET_PostFrame()
{
  // Wake up texture asset loading thread
  if (APP.ast.tex_load_needed)
  {
    SDL_SignalSemaphore(APP.ast.tex_sem);
    APP.ast.tex_load_needed = false;
  }

  // Increment loaded_t
  ForArray(i, APP.ast.textures)
  {
    ASSET_Texture *asset = APP.ast.textures + i;
    float speed = 10.f;
    asset->b.loaded_t += (float)asset->b.loaded * APP.dt * speed;
    asset->b.loaded_t = Min(1.f, asset->b.loaded_t);
  }
}

static void ASSET_Init()
{
  BREAD_LoadFile("data.bread");
  ASSET_LoadSkeletons();
  ASSET_LoadGeometry();

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

    ForArray(i, APP.ast.textures)
    {
      ASSET_Texture *asset = APP.ast.textures + i;
      asset->handle = APP.ast.tex_fallback;
      asset->shininess = 16.f;
    }
  }
}

static void ASSET_Deinit()
{
  // textures
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.tex_fallback);
  ForArray(i, APP.ast.textures)
    SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.textures[i].handle);

  // geometry
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.rigid_vertices);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.skinned_vertices);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.indices);
}
