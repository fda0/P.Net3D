ASSET_Type :: enum
{
  Material;
  Model;
};

ASSET_Key :: struct
{
  type4_hash60: u64;
  name: string;
  // @info name is used for debugging/logging only -
  // - and might be excluded from shipping builds.
  // The constraint is this string's buffer has to be stored externally and
  // it has to be valid through the whole lifetime of the program.
  // This is fine for now. In the future if we add .pie file reloading we might
  // decide to store these names in some lookup table outside of MATERIAL_Key itself.

  // material name format:
  //   tex.Bricks071
  //   Tree.Bark_diffuse
};

MATERIAL_Key :: #type,isa ASSET_Key;
MODEL_Key ::  #type,isa ASSET_Key;

ASSET_CreateKey :: (type: ASSET_Type, name: string /* EXTERNALLY OWNED STRING */) -> ASSET_Key
{
  hash_state := Hash64_Init(0xdead_beef_1234_5678);
  Hash64_UpdateT(*hash_state, type);
  Hash64_Update(*hash_state, name);
  hash := Hash64_Digest(*hash_state);

  res: ASSET_Key;
  res.type4_hash60 = (cast(u64) type) | (hash << 4);
  res.name = name;
  return res;
}

ASSET_MatchKey :: (a: ASSET_Key, b: ASSET_Key) -> bool
{
  return a.type4_hash60 == b.type4_hash60;
}

ASSET_KeyType :: (key: ASSET_Key) -> ASSET_Type
{
  return cast(ASSET_Type) (key.type4_hash60 & 0b1111);
}

MATERIAL_CreateKey :: (name: string) -> MATERIAL_Key { return cast(MATERIAL_Key) ASSET_CreateKey(.Material, name); }
MODEL_CreateKey    :: (name: string) -> MODEL_Key    { return cast(MODEL_Key)    ASSET_CreateKey(.Model, name); }
MATERIAL_MatchKey :: (a: MATERIAL_Key, b: MATERIAL_Key) -> bool { return ASSET_MatchKey(a, b); }
MODEL_MatchKey    :: (a: MODEL_Key, b: MODEL_Key)       -> bool { return ASSET_MatchKey(a, b); }

ASSET_Streamable :: struct
{
  last_touched_frame: u64;
  loaded_t: float;
  flags: Flags;

  Flags :: enum_flags
  {
    Error;
    Loaded;
  };
}

ASSET_Material :: struct
{
  stream: ASSET_Streamable;
  key: MATERIAL_Key;

  params: PIE_MaterialParams;
  has_texture: bool;
  texture_layers: u32;
  tex: *SDL_GPUTexture;
};

ASSET_Mesh :: struct
{
  material: MATERIAL_Key;
  vertices_start_index: u32;
  indices_start_index: u32;
  indices_count: u32;
};

ASSET_Model :: struct
{
  key: MODEL_Key;
  is_skinned: bool;
  skeleton: *ANIM_Skeleton; // can be null
  meshes: [] ASSET_Mesh;
};

ASSET_PieFile :: struct
{
  pool: Flat_Pool;
  file: string;
  err: bool;

  header: *PIE_Header;
  links: *PIE_Links;
  models: [] PIE_Model;
  materials: [] PIE_Material;
};

ASSET_State :: struct
{
  pie: ASSET_PieFile;

  // Materials
  nil_material: ASSET_Material;
  materials: [] ASSET_Material;

  // Texture loading thread
  tex_load_needed: bool;
  tex_sem: *SDL_Semaphore;

  // Models
  nil_model: ASSET_Model;
  models: [] ASSET_Model;
  model_vertices: *SDL_GPUBuffer;
  model_indices: *SDL_GPUBuffer;
  skeletons: [] ANIM_Skeleton;
};

ASSET_MaterialIsNil :: (a: *ASSET_Material) -> bool
{
  return a == *G.ast.nil_material;
}

ASSET_GetMaterial :: (key: MATERIAL_Key) -> *ASSET_Material
{
  // @speed hash table lookup in the future
  asset := *G.ast.nil_material;
  for *G.ast.materials
  {
    if (MATERIAL_MatchKey(it.key, key))
    {
      asset = it;
      break;
    }
  }

  if !ASSET_MaterialIsNil(asset)
  {
    asset.stream.last_touched_frame = G.frame_number;
    if (!asset.stream.flags & .Loaded) G.ast.tex_load_needed = true;
  }

  return asset;
}

ASSET_PrefetchMaterial :: (key: MATERIAL_Key)
{
  ASSET_GetMaterial(key);
}

ASSET_InitMaterials :: ()
{
  // Init nil material
  {
    G.ast.nil_material.stream.flags |= .Loaded;
    G.ast.nil_material.stream.loaded_t = 1;
    G.ast.nil_material.has_texture = true;
    G.ast.nil_material.texture_layers = 1;

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
    G.ast.nil_material.tex = texture;

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
  }

  // Init material array
  {
    pie := *G.ast.pie;

    G.ast.materials = PoolAllocArray(*pie.pool, ASSET_Material, pie.materials.count, true);
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

//
// Model
//
ASSET_ModelIsNil :: (a: *ASSET_Model) -> bool
{
  return a == *G.ast.nil_model;
}

ASSET_GetModel :: (key: MODEL_Key) -> *ASSET_Model
{
  // @speed hash table lookup in the future
  asset := *G.ast.nil_model;
  for *G.ast.models
  {
    if (MODEL_MatchKey(it.key, key))
    {
      asset = it;
      break;
    }
  }
  return asset;
}
