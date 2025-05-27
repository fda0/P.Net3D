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

ASSET_KeyType :: (key: ASSET_Key) -> ASSET_Type
{
  return cast(ASSET_Type) (key.type4_hash60 & 0b1111);
}

MATERIAL_CreateKey :: (name: string) -> MATERIAL_Key
{
  return cast(MATERIAL_Key) ASSET_CreateKey(.Material, name);
}
MODEL_CreateKey :: (name: string) -> MODEL_Key
{
  return cast(MODEL_Key) ASSET_CreateKey(.Model, name);
}

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
  // params: PIE_MaterialParams;
  has_texture: bool;
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
