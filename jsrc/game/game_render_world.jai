WORLD_Dir :: enum u32
{
  E; // +X
  W; // -X
  N; // +Y
  S; // -Y
  T; // +Z
  B; // -Z
  COUNT;
};

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

  fog_color: Color32; // RGBA
  sky_ambient: Color32; // RGBA
  sun_diffuse: Color32; // RGBA
  sun_specular: Color32; // RGBA

  material_diffuse: Color32; // RGBA
  material_specular: Color32; // RGBA
  material_roughness: float;
  material_loaded_t: float;
};

WORLD_InstanceModel :: struct
{
  transform: Mat4;
  color: Color32;
  pose_offset: u32; // in indices; unused for rigid
};

WORLD_ApplyMaterialToUniform :: (uniform: *WORLD_Uniform, material: *ASSET_Material)
{
  uniform.material_loaded_t = material.stream.loaded_t;
  uniform.material_diffuse = xx material.params.diffuse;
  uniform.material_specular = xx material.params.specular;
  uniform.material_roughness = xx material.params.roughness;

  uniform.flags = .ApplyShadows;
  if material.has_texture
  {
    if material.texture_layers >= 1  uniform.flags |= .SampleTexDiffuse;
    if material.texture_layers >= 2  uniform.flags |= .SampleTexNormal;
    if material.texture_layers >= 3  uniform.flags |= .SampleTexRoughness;
  }
}

WORLD_DrawModel :: (model_key: MODEL_Key, transform: Mat4, color: Color32, animation_index: s32, animation_t: float)
{
  instance := WORLD_InstanceModel.{transform = transform, color = color};
  model := ASSET_GetModel(model_key);
  if model.is_skinned
  {
    instance.pose_offset = GPU_BATCH_GetPosesBatch().element_count;

    transforms := ANIM_GetPoseTransforms(model.skeleton, animation_index, animation_t);
    transfer_size := transforms.count * size_of(type_of(transforms[0]));
    GPU_BATCH_TransferUploadBytes(GPU_BATCH_GetPosesBatch(), transforms.data, xx transfer_size, xx transforms.count);
  }

  instance_batch := GPU_BATCH_FindOrCreateBundle(.{type = .ModelInstances, model_key = model_key});
  GPU_BATCH_TransferUploadBytes(instance_batch, *instance, size_of(type_of(instance)), 1);
}

WORLD_DrawVertices :: (material: MATERIAL_Key, vertices: *WORLD_Vertex, vertices_count: u32)
{
  mesh_batch := GPU_BATCH_FindOrCreateBundle(.{type=.MeshVertices, material_key=material});
  transfer_size: u32 = vertices_count * size_of(type_of(vertices[0]));
  GPU_BATCH_TransferUploadBytes(mesh_batch, vertices, transfer_size, vertices_count);
}

