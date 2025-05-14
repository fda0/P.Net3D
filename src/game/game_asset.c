//
// Material
//
static bool ASSET_MaterialIsNil(ASSET_Material *a)
{
  return a == &APP.ast.nil_material;
}

static ASSET_Material *ASSET_GetMaterial(MATERIAL_Key key)
{
  // @speed hash table lookup in the future
  ASSET_Material *asset = &APP.ast.nil_material;
  ForU32(i, APP.ast.materials_count)
  {
    ASSET_Material *search = APP.ast.materials + i;
    if (MATERIAL_KeyMatch(search->key, key))
    {
      asset = search;
      break;
    }
  }

  if (!ASSET_MaterialIsNil(asset))
  {
    asset->b.last_touched_frame = APP.frame_id;
    if (!asset->b.loaded) APP.ast.tex_load_needed = true;
  }

  return asset;
}

static void ASSET_PrefetchMaterial(MATERIAL_Key key)
{
  ASSET_GetMaterial(key);
}

static void ASSET_CreateNilMaterial()
{
  APP.ast.nil_material.b.loaded = true;
  APP.ast.nil_material.b.loaded_t = 1.f;
  APP.ast.nil_material.has_texture = true;

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
  APP.ast.nil_material.tex = texture;

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

static void ASSET_LoadMaterials()
{
  ASSET_PieFile *pie = &APP.ast.pie;

  APP.ast.materials_count = pie->materials_count;
  APP.ast.materials = AllocZeroed(pie->arena, ASSET_Material, pie->materials_count);

  ForU32(i, APP.ast.materials_count)
  {
    PIE_Material *pie_material = pie->materials + i;
    ASSET_Material *asset = APP.ast.materials + i;

    asset->key = MATERIAL_CreateKey(PIE_ListToS8(pie_material->name));

    asset->diffuse = pie_material->diffuse;
    asset->specular = pie_material->specular;
    asset->shininess = pie_material->shininess;

    asset->has_texture = !!pie_material->tex.format;
    if (!asset->has_texture)
    {
      // no texture = asset is already fully loaded
      asset->b.loaded = true;
      asset->b.loaded_t = 1.f;
    }

    asset->tex = APP.ast.nil_material.tex; // fallback texture as default in case anybody tries to use it
  }
}



static void ASSET_LoadMaterialFromPieFile(U32 material_index, U64 min_frame)
{
  Assert(material_index < APP.ast.materials_count);
  ASSET_Material *asset = APP.ast.materials + material_index;
  if (asset->b.loaded || asset->b.last_touched_frame < min_frame) return;

  ASSET_PieFile *br = &APP.ast.pie;
  // Material from .pie - it contains a big buffer that needs to uploaded to GPU
  PIE_Material *br_material = br->materials + material_index;
  S8 gpu_full_data = PIE_ListToS8(br_material->tex.full_data);

  // MaterialTextures - it contains a mapping from big buffer to individual pieces of texture layers and lods
  U32 br_textures_count = br_material->tex.sections.count;
  PIE_MaterialTexSection *br_textures = PIE_ListAsType(br_material->tex.sections, PIE_MaterialTexSection);

  U32 lods_count = br_material->tex.lods;
  bool generate_lods = false;
  if (lods_count == 1 && br_material->tex.format != PIE_Tex_BC7_RGBA)
  {
    lods_count = CalculateMipMapCount(br_material->tex.width, br_material->tex.height);
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
      SDL_GPUTextureFormat sdl_format = (br_material->tex.format == PIE_Tex_R8G8B8A8 ?
                                         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM :
                                         SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM);

      SDL_GPUTextureCreateInfo texture_info =
      {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        .format = sdl_format,
        .width = br_material->tex.width,
        .height = br_material->tex.height,
        .layer_count_or_depth = br_material->tex.layers,
        .num_levels = lods_count,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
      };

      if (generate_lods)
        texture_info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

      texture = SDL_CreateGPUTexture(APP.gpu.device, &texture_info);
      SDL_SetGPUTextureName(APP.gpu.device, texture, "Texture from pie");
    }

    // GPU memory -> GPU texture
    {
      SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
      SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

      ForU32(br_texture_index, br_textures_count)
      {
        PIE_MaterialTexSection *br_sect = br_textures + br_texture_index;

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

  asset->tex = texture;
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

    ForU32(material_index, APP.ast.materials_count)
      ASSET_LoadMaterialFromPieFile(material_index, min_frame);

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
  ASSET_PieFile *br = &APP.ast.pie;

  U32 br_skeletons_count = br->links->skeletons.count;
  PIE_Skeleton *br_skeletons = PIE_ListAsType(br->links->skeletons, PIE_Skeleton);

  U32 skeletons_count = Min(br_skeletons_count, ArrayCount(APP.ast.skeletons));
  APP.ast.skeletons_count = skeletons_count;

  ForU32(skeleton_index, skeletons_count)
  {
    PIE_Skeleton *br_skel = br_skeletons + skeleton_index;
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

    skel->inv_bind_mats      = PIE_ListAsType(br_skel->inv_bind_mats, Mat4);
    skel->child_index_buf    = PIE_ListAsType(br_skel->child_index_buf, U32);
    skel->child_index_ranges = PIE_ListAsType(br_skel->child_index_ranges, RngU32);
    skel->translations       = PIE_ListAsType(br_skel->translations, V3);
    skel->rotations          = PIE_ListAsType(br_skel->rotations, Quat);
    skel->scales             = PIE_ListAsType(br_skel->scales, V3);

    RngU32 *name_ranges = PIE_ListAsType(br_skel->name_ranges, RngU32);
    skel->names_s8 = Alloc(br->arena, S8, skel->joints_count);
    ForU32(i, skel->joints_count)
      skel->names_s8[i] = S8_Substring(PIE_File(), name_ranges[i].min, name_ranges[i].max);

    // Animations
    skel->anims_count = br_skel->anims.count;
    PIE_Animation *br_anims = PIE_ListAsType(br_skel->anims, PIE_Animation);
    skel->anims = Alloc(br->arena, ASSET_Animation, skel->anims_count);

    ForU32(anim_index, skel->anims_count)
    {
      ASSET_Animation *anim = skel->anims + anim_index;
      PIE_Animation *br_anim = br_anims + anim_index;

      anim->name_s8 = S8_Make(PIE_ListAsType(br_anim->name, U8), br_anim->name.size);

      anim->t_min = br_anim->t_min;
      anim->t_max = br_anim->t_max;

      anim->channels_count = br_anim->channels.count;
      PIE_AnimationChannel *br_channels = PIE_ListAsType(br_anim->channels, PIE_AnimationChannel);
      anim->channels = Alloc(br->arena, ASSET_AnimationChannel, anim->channels_count);

      ForU32(channel_index, anim->channels_count)
      {
        ASSET_AnimationChannel *chan = anim->channels + channel_index;
        PIE_AnimationChannel *br_chan = br_channels + channel_index;

        chan->joint_index = br_chan->joint_index;
        chan->type = br_chan->type;
        chan->count = br_chan->inputs.count;

        U32 comp_count = (chan->type == AN_Rotation ? 4 : 3);
        Assert(comp_count * chan->count == br_chan->outputs.count);

        chan->inputs = PIE_ListAsType(br_chan->inputs, float);
        chan->outputs = PIE_ListAsType(br_chan->outputs, float);
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
  ASSET_PieFile *br = &APP.ast.pie;

  APP.ast.vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                      br->links->models.vertices.size,
                                      "WORLD vertices");
  APP.ast.indices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX,
                                     br->links->models.indices.size,
                                     "Model indices");

  S8 vertices_string = PIE_ListToS8(br->links->models.vertices);
  S8 indices_string = PIE_ListToS8(br->links->models.indices);

  GPU_TransferBuffer(APP.ast.vertices, vertices_string.str, vertices_string.size);
  GPU_TransferBuffer(APP.ast.indices, indices_string.str, indices_string.size);

  ForU32(model_kind, MODEL_COUNT)
  {
    PIE_Model *br_model = br->models + model_kind;
    ASSET_Model *asset = APP.ast.models + model_kind;
    asset->is_skinned = br_model->is_skinned;
    asset->skeleton_index = br_model->skeleton_index;
    asset->geos_count = br_model->geometries.count;
    asset->geos = AllocZeroed(br->arena, ASSET_Geometry, asset->geos_count);

    PIE_Geometry *br_geos = PIE_ListAsType(br_model->geometries, PIE_Geometry);

    ForU32(geo_index, asset->geos_count)
    {
      PIE_Geometry *br_geo = br_geos + geo_index;
      ASSET_Geometry *geo = asset->geos + geo_index;
      geo->color = br_geo->color;
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
  ForU32(i, APP.ast.materials_count)
  {
    ASSET_Material *asset = APP.ast.materials + i;
    float speed = 10.f;
    asset->b.loaded_t += (float)asset->b.loaded * APP.dt * speed;
    asset->b.loaded_t = Min(1.f, asset->b.loaded_t);
  }
}

static void ASSET_Init()
{
  PIE_LoadFile("data.pie");
  ASSET_LoadSkeletons();
  ASSET_LoadGeometry();
  ASSET_CreateNilMaterial();
  ASSET_LoadMaterials();
}

static void ASSET_Deinit()
{
  // textures
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.nil_material.tex);
  ForArray(i, APP.ast.materials)
  {
    ASSET_Material *mat = APP.ast.materials + i;
    if (mat->has_texture && mat->b.loaded)
      SDL_ReleaseGPUTexture(APP.gpu.device, mat->tex);
  }

  // geometry
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.vertices);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.indices);
}
