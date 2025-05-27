WORLD_ShaderFlags :: enum_flags u32 // @todo generate hlsl header for shaders out of this
{
  DoMeshSkinning;
  UseInstanceBuffer;
  SampleTexDiffuse;
  SampleTexNormal;
  SampleTexRoughness;
  ApplyShadows;
  DrawBorderAtUVEdge;
};

WORLD_Uniform :: struct
{
  camera_transform: Mat4;
  shadow_transform: Mat4;
  camera_position: V3 #align 16;
  sun_dir: V3 #align 16;
  flags: WORLD_ShaderFlags;

  fog_color: u32; // RGBA
  sky_ambient: u32; // RGBA
  sun_diffuse: u32; // RGBA
  sun_specular: u32; // RGBA

  material_diffuse: u32; // RGBA
  material_specular: u32; // RGBA
  material_roughness: float;
  material_loaded_t: float;
};

WORLD_Vertex :: struct
{
  p: V3;
  normal: V3;
  uv: V2;
  joints_packed4: u32;
  joint_weights: V4;
};

WORLD_InstanceModel :: struct
{
  transform: Mat4;
  color: u32;
  pose_offset: u32; // in indices; unused for rigid
};

WORLD_ApplyMaterialToUniform :: (uniform: *WORLD_Uniform, material: *ASSET_Material)
{
  assert(false, "@impl");
}