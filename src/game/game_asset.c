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
  APP.ast.nil_material.texture_layers = 1;

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
    asset->params = pie_material->params;
    asset->has_texture = !!pie_material->tex.format;
    asset->texture_layers = pie_material->tex.layers;
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

  // Material from .pie - it contains a big buffer that needs to uploaded to GPU
  ASSET_PieFile *pie = &APP.ast.pie;
  PIE_Material *pie_material = pie->materials + material_index;
  S8 gpu_full_data = PIE_ListToS8(pie_material->tex.full_data);

  // MaterialTextures - it contains a mapping from big buffer to individual pieces of texture layers and lods
  U32 pie_textures_count = pie_material->tex.sections.count;
  PIE_MaterialTexSection *pie_textures = PIE_ListAsType(pie_material->tex.sections, PIE_MaterialTexSection);

  U32 lods_count = pie_material->tex.lods;
  bool generate_lods = false;
  if (lods_count == 1 && pie_material->tex.format != PIE_Tex_BC7_RGBA)
  {
    lods_count = CalculateMipMapCount(pie_material->tex.width, pie_material->tex.height);
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
      SDL_GPUTextureFormat sdl_format = (pie_material->tex.format == PIE_Tex_R8G8B8A8 ?
                                         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM :
                                         SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM);

      SDL_GPUTextureCreateInfo texture_info =
      {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        .format = sdl_format,
        .width = pie_material->tex.width,
        .height = pie_material->tex.height,
        .layer_count_or_depth = pie_material->tex.layers,
        .num_levels = lods_count,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
      };

      if (generate_lods)
        texture_info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

      texture = SDL_CreateGPUTexture(APP.gpu.device, &texture_info);

      Pr_MakeOnStack(p, Kilobyte(1));
      Pr_Cstr(&p, "PIE Texture: ");
      Pr_S8(&p, PIE_ListToS8(pie_material->name));
      char *debug_texture_name = Pr_AsCstr(&p);
      SDL_SetGPUTextureName(APP.gpu.device, texture, debug_texture_name);
    }

    // GPU memory -> GPU texture
    {
      SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
      SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

      ForU32(pie_texture_index, pie_textures_count)
      {
        PIE_MaterialTexSection *pie_sect = pie_textures + pie_texture_index;

        SDL_GPUTextureTransferInfo source =
        {
          .transfer_buffer = transfer,
          .offset = pie_sect->data_offset,
          .pixels_per_row = pie_sect->width,
        };
        SDL_GPUTextureRegion destination =
        {
          .texture = texture,
          .mip_level = pie_sect->lod,
          .layer = pie_sect->layer,
          .w = pie_sect->width,
          .h = pie_sect->height,
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
  ASSET_PieFile *pie = &APP.ast.pie;
  PIE_Skeleton *pie_skeletons = PIE_ListAsType(pie->links->skeletons, PIE_Skeleton);
  U32 skeletons_count = pie->links->skeletons.count;

  APP.ast.skeletons_count = skeletons_count;
  APP.ast.skeletons = AllocZeroed(pie->arena, ASSET_Skeleton, skeletons_count);

  ForU32(skeleton_index, skeletons_count)
  {
    PIE_Skeleton *pie_skel = pie_skeletons + skeleton_index;
    ASSET_Skeleton *skel = APP.ast.skeletons + skeleton_index;
    skel->root_transform = pie_skel->root_transform;

    skel->joints_count = pie_skel->inv_bind_mats.count;
    Assert(skel->joints_count == pie_skel->inv_bind_mats.count);
    Assert(skel->joints_count == pie_skel->child_index_buf.count);
    Assert(skel->joints_count == pie_skel->child_index_ranges.count);
    Assert(skel->joints_count == pie_skel->translations.count);
    Assert(skel->joints_count == pie_skel->rotations.count);
    Assert(skel->joints_count == pie_skel->scales.count);
    Assert(skel->joints_count == pie_skel->name_ranges.count);

    skel->inv_bind_mats      = PIE_ListAsType(pie_skel->inv_bind_mats, Mat4);
    skel->child_index_buf    = PIE_ListAsType(pie_skel->child_index_buf, U32);
    skel->child_index_ranges = PIE_ListAsType(pie_skel->child_index_ranges, RngU32);
    skel->translations       = PIE_ListAsType(pie_skel->translations, V3);
    skel->rotations          = PIE_ListAsType(pie_skel->rotations, Quat);
    skel->scales             = PIE_ListAsType(pie_skel->scales, V3);

    RngU32 *name_ranges = PIE_ListAsType(pie_skel->name_ranges, RngU32);
    skel->names_s8 = Alloc(pie->arena, S8, skel->joints_count);
    ForU32(i, skel->joints_count)
      skel->names_s8[i] = S8_Substring(PIE_File(), name_ranges[i].min, name_ranges[i].max);

    // Animations
    skel->anims_count = pie_skel->anims.count;
    PIE_Animation *pie_anims = PIE_ListAsType(pie_skel->anims, PIE_Animation);
    skel->anims = Alloc(pie->arena, ASSET_Animation, skel->anims_count);

    ForU32(anim_index, skel->anims_count)
    {
      ASSET_Animation *anim = skel->anims + anim_index;
      PIE_Animation *pie_anim = pie_anims + anim_index;

      anim->name = S8_Make(PIE_ListAsType(pie_anim->name, U8), pie_anim->name.size);

      anim->t_min = pie_anim->t_min;
      anim->t_max = pie_anim->t_max;

      anim->channels_count = pie_anim->channels.count;
      PIE_AnimationChannel *pie_channels = PIE_ListAsType(pie_anim->channels, PIE_AnimationChannel);
      anim->channels = Alloc(pie->arena, ASSET_AnimationChannel, anim->channels_count);

      ForU32(channel_index, anim->channels_count)
      {
        ASSET_AnimationChannel *chan = anim->channels + channel_index;
        PIE_AnimationChannel *pie_chan = pie_channels + channel_index;

        chan->joint_index = pie_chan->joint_index;
        chan->type = pie_chan->type;
        chan->count = pie_chan->inputs.count;

        U32 comp_count = (chan->type == AN_Rotation ? 4 : 3);
        Assert(comp_count * chan->count == pie_chan->outputs.count);

        chan->inputs = PIE_ListAsType(pie_chan->inputs, float);
        chan->outputs = PIE_ListAsType(pie_chan->outputs, float);
      }
    }
  }
}

//
// Geometry
//
static void ASSET_CreateNilModel()
{
  // @todo create some obviously wrong mesh; error message?
}

static bool ASSET_ModelIsNil(ASSET_Model *a)
{
  return a == &APP.ast.nil_model;
}

static ASSET_Model *ASSET_GetModel(MODEL_Key key)
{
  // @speed hash table lookup in the future
  ASSET_Model *asset = &APP.ast.nil_model;
  ForU32(i, APP.ast.models_count)
  {
    ASSET_Model *search = APP.ast.models + i;
    if (MODEL_KeyMatch(search->key, key))
    {
      asset = search;
      break;
    }
  }
  return asset;
}

static void ASSET_LoadModelMeshes()
{
  ASSET_PieFile *pie = &APP.ast.pie;

  // Load meshes
  {
    APP.ast.vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                        pie->links->models.vertices.size,
                                        "WORLD vertices");
    APP.ast.indices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX,
                                       pie->links->models.indices.size,
                                       "Model indices");

    S8 vertices_string = PIE_ListToS8(pie->links->models.vertices);
    S8 indices_string = PIE_ListToS8(pie->links->models.indices);

    GPU_TransferBuffer(APP.ast.vertices, vertices_string.str, vertices_string.size);
    GPU_TransferBuffer(APP.ast.indices, indices_string.str, indices_string.size);
  }

  APP.ast.models_count = pie->models_count;
  APP.ast.models = AllocZeroed(pie->arena, ASSET_Model, APP.ast.models_count);

  ForU32(model_index, pie->models_count)
  {
    PIE_Model *pie_model = pie->models + model_index;

    ASSET_Model *asset = APP.ast.models + model_index;
    asset->key = MODEL_CreateKey(PIE_ListToS8(pie_model->name));
    asset->is_skinned = pie_model->is_skinned;
    asset->skeleton_index = pie_model->skeleton_index;
    asset->meshes_count = pie_model->meshes.count;
    asset->meshes = AllocZeroed(pie->arena, ASSET_Mesh, asset->meshes_count);

    PIE_Mesh *pie_meshes = PIE_ListAsType(pie_model->meshes, PIE_Mesh);
    ForU32(mesh_index, asset->meshes_count)
    {
      PIE_Mesh *pie_mesh = pie_meshes + mesh_index;
      ASSET_Mesh *mesh = asset->meshes + mesh_index;

      if (pie_mesh->material_index < APP.ast.materials_count)
      {
        ASSET_Material *material = APP.ast.materials + pie_mesh->material_index;
        mesh->material = material->key;
      }
      mesh->vertices_start_index = pie_mesh->vertices_start_index;
      mesh->indices_start_index = pie_mesh->indices_start_index;
      mesh->indices_count = pie_mesh->indices_count;
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
  ASSET_LoadPieFile("data.pie");
  ASSET_LoadSkeletons();
  ASSET_CreateNilMaterial();
  ASSET_LoadMaterials();
  ASSET_CreateNilModel();
  ASSET_LoadModelMeshes();
}

static void ASSET_Deinit()
{
  // textures
  SDL_ReleaseGPUTexture(APP.gpu.device, APP.ast.nil_material.tex);
  ForU32(i, APP.ast.materials_count)
  {
    ASSET_Material *mat = APP.ast.materials + i;
    if (mat->has_texture && mat->b.loaded)
      SDL_ReleaseGPUTexture(APP.gpu.device, mat->tex);
  }

  // geometry
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.vertices);
  SDL_ReleaseGPUBuffer(APP.gpu.device, APP.ast.indices);
}
