ASSET_State :: struct
{
  pie: ASSET_PieFile;
  arena: Arena;

  // Materials
  nil_material: ASSET_Material;
  materials: [] ASSET_Material;

  // Texture loading thread
  tex_load_needed: bool;
  tex_semaphore: Semaphore;
  tex_thread: Thread;

  // Models
  nil_model: ASSET_Model;
  models: [] ASSET_Model;
  model_vertices: *SDL_GPUBuffer;
  model_indices: *SDL_GPUBuffer;
  skeletons: [] ASSET_Skeleton;

  // Shaders
  shaders: [] string;
};

ASSET_ShaderType :: enum
{
  WORLD_VS;
  WORLD_PS;
  UI_VS;
  UI_PS;
};

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
  hash := Hash64Any(type, name);

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

ASSET_IsZeroKey :: (key: ASSET_Key) -> bool
{
  return !key.type4_hash60;
}

MATERIAL_CreateKey :: (name: string) -> MATERIAL_Key { return cast(MATERIAL_Key) ASSET_CreateKey(.Material, name); }
MODEL_CreateKey    :: (name: string) -> MODEL_Key    { return cast(MODEL_Key)    ASSET_CreateKey(.Model, name); }
MATERIAL_MatchKey :: (a: MATERIAL_Key, b: MATERIAL_Key) -> bool { return ASSET_MatchKey(a, b); }
MODEL_MatchKey    :: (a: MODEL_Key, b: MODEL_Key)       -> bool { return ASSET_MatchKey(a, b); }
MATERIAL_IsZeroKey :: ASSET_IsZeroKey;
MODEL_IsZeroKey    :: ASSET_IsZeroKey;

ASSET_Streamable :: struct
{
  last_touched_frame: s64;
  loaded_t: float;
  flags: Flags;

  Flags :: enum_flags
  {
    ERROR;
    LOADED;
  };
}

ASSET_Material :: struct
{
  stream: ASSET_Streamable;
  key: MATERIAL_Key;

  params: PIE_MaterialParams;
  has_texture: bool;
  texture_layers: s32;
  tex: *SDL_GPUTexture;
};

ASSET_Mesh :: struct
{
  material: MATERIAL_Key;
  vertices_start_index: u32;
  indices_start_index: u32;
  indices_count: u32;
};

ASSET_AnimationChannel :: struct
{
  type: PIE_AnimationChannelType;
  joint_index: u32;
  inputs: [] float;
  outputs: [] float; // .count = inputs.count * (ifx type == .Rotation then 4 else 3);
};

ASSET_Animation :: struct
{
  name: string;
  t_min, t_max: float;
  channels: [] ASSET_AnimationChannel;
};

ASSET_Skeleton :: struct
{
  root_transform: Mat4;
  animations: [] ASSET_Animation;

  joints_count: u32; // all arrays below have count equal to joints_count
  joint_names: [] string;
  inverse_matrices: [] Mat4; // inverse bind matrices
  child_index_buffer: [] u32;
  child_index_ranges: [] Range(u32);

  bind_translations: [] V3;
  bind_rotations: [] Quat;
  bind_scales: [] V3;
};

ASSET_Model :: struct
{
  key: MODEL_Key;
  is_skinned: bool;
  skeleton: *ASSET_Skeleton; // can be null
  meshes: [] ASSET_Mesh;
};

ASSET_PieFile :: struct
{
  file: string;
  err: bool;

  header: *PIE_Header;
  links: *PIE_Links;
  models: [] PIE_Model;
  materials: [] PIE_Material;
  shaders: [] PIE_Shader;
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
    if !(asset.stream.flags & .LOADED)  G.ast.tex_load_needed = true;
  }

  return asset;
}

ASSET_PrefetchMaterial :: (key: MATERIAL_Key)
{
  ASSET_GetMaterial(key);
}

// Model
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

AnimationNameToIndex :: (skeleton: ASSET_Skeleton, name: string) -> index: s32, found: bool
{
  for skeleton.animations
  {
    if name == it.name
      return xx it_index, true;
  }
  log("Animation name not found: %", name, flags=.WARNING);
  return 0, false;
}

JointNameToIndex :: (skeleton: ASSET_Skeleton, name: string) -> index: u32, found: bool
{
  for skeleton.joint_names
  {
    if name == it
      return xx it_index, true;
  }
  log("Joint name not found: %", name, flags=.WARNING);
  return 0, false;
}

// Shader
ASSET_GetShader :: (type: ASSET_ShaderType) -> string
{
  if xx type >= G.ast.shaders.count then return "";
  return G.ast.shaders[type];
}