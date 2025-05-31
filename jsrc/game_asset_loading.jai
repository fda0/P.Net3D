// MATERIALS
ASSET_InitMaterials :: ()
{
  // Init nil material
  {
    dim: u32 = 512;
    tex_size: u32 = dim * dim * size_of(u32);
    and_mask: u32 = (1 << 6);

    tex_info := SDL_GPUTextureCreateInfo.{
      type = .SDL_GPU_TEXTURETYPE_2D_ARRAY,
      format = .SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      width = dim,
      height = dim,
      layer_count_or_depth = 1,
      num_levels = CalculateMipMapCount(dim, dim),
      usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
    };
    texture := SDL_CreateGPUTexture(G.gpu.device, *tex_info);
    SDL_SetGPUTextureName(G.gpu.device, texture, "Fallback texture");

    // Fill texture
    {
      trans_desc := SDL_GPUTransferBufferCreateInfo.{
        usage = .SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        size = tex_size,
      };
      trans_buf := SDL_CreateGPUTransferBuffer(G.gpu.device, *trans_desc);
      assert(!!trans_buf, "@todo handle this error? or remove assert");

      // Fill GPU memory
      {
        map: *u32 = SDL_MapGPUTransferBuffer(G.gpu.device, trans_buf, false);
        pixel := map;
        for y: 0..dim-1
        {
          for x: 0..dim-1
          {
            pixel[0] = xx ifx x & y & and_mask then 0xff000000 else 0xffFF00FF;
            pixel += 1;
          }
        }
        SDL_UnmapGPUTransferBuffer(G.gpu.device, trans_buf);
      }

      // GPU memory -> GPU buffers
      {
        cmd := SDL_AcquireGPUCommandBuffer(G.gpu.device);
        copy_pass := SDL_BeginGPUCopyPass(cmd);

        trans_info := SDL_GPUTextureTransferInfo.{transfer_buffer = trans_buf};
        dst_region := SDL_GPUTextureRegion.{
          texture = texture,
          w = dim,
          h = dim,
          d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, *trans_info, *dst_region, false);

        SDL_EndGPUCopyPass(copy_pass);
        SDL_SubmitGPUCommandBuffer(cmd);
      }
      SDL_ReleaseGPUTransferBuffer(G.gpu.device, trans_buf);
    }

    // Generate texture mipmap
    {
      cmd := SDL_AcquireGPUCommandBuffer(G.gpu.device);
      SDL_GenerateMipmapsForGPUTexture(cmd, texture);
      SDL_SubmitGPUCommandBuffer(cmd);
    }

    // Fill nil_material data
    {
      G.ast.nil_material.stream.flags |= .Loaded;
      G.ast.nil_material.stream.loaded_t = 1;
      G.ast.nil_material.has_texture = true;
      G.ast.nil_material.texture_layers = 1;
      G.ast.nil_material.tex = texture;
    }
  }

  // Init material array
  {
    using,only(pie) G.ast;

    G.ast.materials = NewArray(pie.materials.count, ASSET_Material);
    for *G.ast.materials
    {
      pie_material := *pie.materials[it_index];
      it.key = MATERIAL_CreateKey(PIE_LOAD_ListToString(pie_material.name));
      it.params = pie_material.params;
      it.has_texture = pie_material.tex.format != .Empty;
      it.texture_layers = pie_material.tex.layers;
      if (!it.has_texture)
      {
        // no texture = it is already fully loaded
        it.stream.flags |= .Loaded;
        it.stream.loaded_t = 1;
      }
      it.tex = G.ast.nil_material.tex; // fallback texture as default in case anybody tries to use it
    }
  }
}

// MODELS
ASSET_InitModels :: ()
{
  using,only(pie) G.ast;

  // Load meshes
  {
    G.ast.model_vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
      pie.links.models.vertices.size,
      "WORLD vertices");
    G.ast.model_indices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX,
      pie.links.models.indices.size,
      "Model indices");

    vertices_string := PIE_LOAD_ListToString(pie.links.models.vertices);
    indices_string := PIE_LOAD_ListToString(pie.links.models.indices);

    GPU_TransferBuffer(G.ast.model_vertices, vertices_string.data, xx vertices_string.count);
    GPU_TransferBuffer(G.ast.model_indices, indices_string.data, xx indices_string.count);
  }

  G.ast.models = NewArray(pie.models.count, ASSET_Model);

  for *model: G.ast.models
  {
    pie_model := *pie.models[it_index];

    model.key = MODEL_CreateKey(PIE_LOAD_ListToString(pie_model.name));
    model.is_skinned = !!pie_model.is_skinned;

    if PIE_LOAD_Check(pie_model.skeleton_index < G.ast.skeletons.count,
      tprint("Pie model skeleton index (%) is out of bounds (count=%).", pie_model.skeleton_index, G.ast.skeletons.count),
      critical = false)
    {
      model.skeleton = *G.ast.skeletons[pie_model.skeleton_index];
    }

    model.meshes = NewArray(pie_model.meshes.count, ASSET_Mesh);

    pie_meshes := PIE_LOAD_ListToArray(pie_model.meshes, PIE_Mesh);
    for *mesh: model.meshes
    {
      pie_mesh := *pie_meshes[it_index];

      if PIE_LOAD_Check(pie_mesh.material_index < G.ast.materials.count,
        tprint("Pie mesh material index (%) is out of bounds (count=%).", pie_mesh.material_index, G.ast.materials.count),
        critical = false)
      {
        mesh.material = G.ast.materials[pie_mesh.material_index].key;
      }

      mesh.vertices_start_index = pie_mesh.vertices_start_index;
      mesh.indices_start_index = pie_mesh.indices_start_index;
      mesh.indices_count = pie_mesh.indices_count;
    }
  }
}

// SKELETONS
ASSET_InitSkeletons :: ()
{
  using,only(pie) G.ast;

  pie_skeletons := PIE_LOAD_ListToArray(pie.links.skeletons, PIE_Skeleton);
  G.ast.skeletons = NewArray(pie_skeletons.count, ASSET_Skeleton);

  for *skel: G.ast.skeletons
  {
    pie_skel := *pie_skeletons[it_index];
    skel.root_transform = pie_skel.root_transform;

    skel.joints_count       = xx pie_skel.inverse_matrices.count;
    skel.inverse_matrices   = PIE_LOAD_ListToArray(pie_skel.inverse_matrices, Mat4);
    skel.child_index_buffer = PIE_LOAD_ListToArray(pie_skel.child_index_buffer, s32);
    skel.child_index_ranges = PIE_LOAD_ListToArray(pie_skel.child_index_ranges, Range(s32));
    skel.bind_translations  = PIE_LOAD_ListToArray(pie_skel.translations, V3);
    skel.bind_rotations     = PIE_LOAD_ListToArray(pie_skel.rotations, Quat);
    skel.bind_scales        = PIE_LOAD_ListToArray(pie_skel.scales, V3);
    name_ranges            := PIE_LOAD_ListToArray(pie_skel.name_ranges, Range(u32));
    PIE_LOAD_Check(skel.joints_count == skel.inverse_matrices.count, "joints_count != inverse_matrices.count");
    PIE_LOAD_Check(skel.joints_count == skel.child_index_buffer.count, "joints_count != child_index_buffer.count");
    PIE_LOAD_Check(skel.joints_count == skel.child_index_ranges.count, "joints_count != child_index_ranges.count");
    PIE_LOAD_Check(skel.joints_count == skel.bind_translations.count, "joints_count != bind_translations.count");
    PIE_LOAD_Check(skel.joints_count == skel.bind_rotations.count, "joints_count != bind_rotations.count");
    PIE_LOAD_Check(skel.joints_count == skel.bind_scales.count, "joints_count != bind_scales.count");
    PIE_LOAD_Check(skel.joints_count == name_ranges.count, "joints_count != name_ranges.count");
    if pie.err  return;

    skel.joint_names = NewArray(name_ranges.count, string);
    for *skel.joint_names
      it.* = STR_Substring(PIE_LOAD_File(), name_ranges[it_index].min, name_ranges[it_index].max);

    // Animations
    pie_anims := PIE_LOAD_ListToArray(pie_skel.anims, PIE_Animation);
    skel.animations = NewArray(pie_anims.count, ASSET_Animation);

    for *anim: skel.animations
    {
      pie_anim := *pie_anims[it_index];

      anim.name = PIE_LOAD_ListToString(pie_anim.name);
      anim.t_min = pie_anim.t_min;
      anim.t_max = pie_anim.t_max;

      pie_channels := PIE_LOAD_ListToArray(pie_anim.channels, PIE_AnimationChannel);
      anim.channels = NewArray(pie_anim.channels.count, ASSET_AnimationChannel);

      for *chan: anim.channels
      {
        pie_chan := *pie_channels[it_index];

        pie_joint_index := ((pie_chan.joint_index30_type2 << 2) >> 2).(s32);
        pie_transform_type := cast(PIE_AnimationChannelType) (pie_chan.joint_index30_type2 >> 30);
        chan.joint_index = pie_joint_index;
        chan.type = pie_transform_type;
        chan.inputs = PIE_LOAD_ListToArray(pie_chan.inputs, float);

        comp_count := ifx chan.type == .Rotation then 4 else 3;
        PIE_LOAD_Check(comp_count * chan.inputs.count * size_of(float) == pie_chan.outputs.size,
          tprint("Animation channel %*%*% != %", comp_count, chan.inputs.count, size_of(float), pie_chan.outputs.size));
        chan.outputs = PIE_LOAD_ListToArray(pie_chan.outputs, float);
      }
    }
  }
}

// LOAD .PIE
ASSET_LoadPieFile :: (file_path: string)
{
  using,only(pie) G.ast;
  using,only(err) pie;

  file, read_success := read_entire_file(file_path);
  err |= !read_success;
  if err return;
  pie.file = file;

  header := PIE_LOAD_OffsetSizeToArray(0, size_of(PIE_Header), PIE_Header);
  if err return;
  pie.header = *header[0];

  hashable := STR_Skip(PIE_LOAD_File(), size_of(type_of(pie.header.file_hash)));
  calculated_hash := Hash64Any(PIE_MAGIC_HASH_SEED, hashable);
  PIE_LOAD_Check(calculated_hash == pie.header.file_hash,
    tprint("Calculated hash (%) doesn't match file hash (%).", calculated_hash, pie.header.file_hash));
  if err return;

  links := PIE_LOAD_ListToArray(pie.header.links, PIE_Links);
  if err return;
  pie.links = *links[0];

  models := PIE_LOAD_ListToArray(pie.links.models.list, PIE_Model);
  materials := PIE_LOAD_ListToArray(pie.links.materials, PIE_Material);
  if err return;
  pie.models = models;
  pie.materials = materials;
}

// MAIN INIT
ASSET_Init :: ()
{
  init(*G.ast.arena);
  new_context := context;
  new_context.allocator = G.ast.arena;
  push_context new_context
  {
    using,only(err) G.ast.pie;
    if !err  ASSET_LoadPieFile("data.pie");
    if !err  ASSET_InitSkeletons();
    if !err  ASSET_InitMaterials();
    if !err  ASSET_InitModels();
  }
}
