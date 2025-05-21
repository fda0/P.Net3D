static void WORLD_DrawModel(MODEL_Key model_key, Mat4 transform, U32 color,
                            U32 animation_index, float animation_t)
{
  WORLD_InstanceModel instance =
  {
    .transform = transform,
    .color = color,
  };

  ASSET_Model *model = ASSET_GetModel(model_key);
  if (model->is_skinned)
  {
    instance.pose_offset = GPU_MEM_GetPosesBatch()->element_count;

    ASSET_Skeleton *skel = ASSET_GetSkeleton(model->skeleton_index);
    ANIM_Pose pose = ANIM_PoseFromAnimation(skel, animation_index, animation_t);
    GPU_MEM_TransferUploadBytes(GPU_MEM_GetPosesBatch(), pose.mats,
                                pose.mats_count * sizeof(pose.mats[0]),
                                pose.mats_count);
  }

  GPU_MEM_Batch *instance_batch =
    GPU_MEM_FindOrCreateBundle((GPU_MEM_Target){.type = GPU_MEM_ModelInstances, .model_key = model_key});
  GPU_MEM_TransferUploadBytes(instance_batch, &instance, sizeof(instance), 1);
}

static void WORLD_DrawVertices(MATERIAL_Key material, WORLD_Vertex *vertices, U32 vertices_count)
{
  GPU_MEM_Batch *mesh_batch =
    GPU_MEM_FindOrCreateBundle((GPU_MEM_Target){.type = GPU_MEM_MeshVertices, .material_key = material});
  GPU_MEM_TransferUploadBytes(mesh_batch,
                              vertices,
                              vertices_count * sizeof(*vertices),
                              vertices_count);
}

static void WORLD_ApplyMaterialToUniform(WORLD_Uniform *uniform, ASSET_Material *material)
{
  uniform->material_loaded_t = material->b.loaded_t;
  uniform->material_diffuse = material->params.diffuse;
  uniform->material_specular = material->params.specular;
  uniform->material_roughness = material->params.roughness;

  uniform->flags = WORLD_FLAG_ApplyShadows;
  if (material->has_texture)
  {
    if (material->texture_layers >= 1) uniform->flags |= WORLD_FLAG_SampleTexDiffuse;
    if (material->texture_layers >= 2) uniform->flags |= WORLD_FLAG_SampleTexNormal;
    if (material->texture_layers >= 3) uniform->flags |= WORLD_FLAG_SampleTexRoughness;
  }
}
