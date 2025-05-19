//
// WORLD
//
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

//
// UI
//
static U32 UI_ActiveClipIndex()
{
  AssertBounds(APP.gpu.ui.clip_stack_index, APP.gpu.ui.clip_stack);
  U32 clip_index = APP.gpu.ui.clip_stack[APP.gpu.ui.clip_stack_index];
  AssertBounds(clip_index, APP.gpu.ui.clips);
  return clip_index;
}

static UI_Clip UI_ActiveClip()
{
  return APP.gpu.ui.clips[UI_ActiveClipIndex()];
}

static void UI_PushClip(UI_Clip clip)
{
  if (APP.gpu.ui.clip_stack_index + 1 >= ArrayCount(APP.gpu.ui.clip_stack))
    return;
  if (APP.gpu.ui.clips_count >= ArrayCount(APP.gpu.ui.clips))
    return;

  UI_Clip current = UI_ActiveClip();
  // intersection of clip & current
  if (current.p_min.x > clip.p_min.x) clip.p_min.x = current.p_min.x;
  if (current.p_min.y > clip.p_min.y) clip.p_min.y = current.p_min.y;
  if (current.p_max.x < clip.p_max.x) clip.p_max.x = current.p_max.x;
  if (current.p_max.y < clip.p_max.y) clip.p_max.y = current.p_max.y;

  U32 clip_index = APP.gpu.ui.clips_count;
  APP.gpu.ui.clips_count += 1;
  AssertBounds(clip_index, APP.gpu.ui.clips);
  APP.gpu.ui.clips[clip_index] = clip;

  APP.gpu.ui.clip_stack_index += 1;
  APP.gpu.ui.clip_stack[APP.gpu.ui.clip_stack_index] = clip_index;
}

static void UI_PopClip()
{
  if (APP.gpu.ui.clip_stack_index == 0)
    return;
  APP.gpu.ui.clip_stack_index -= 1;
}

static void UI_DrawRaw(UI_Shape shape)
{
  if (APP.gpu.ui.indices_count + 6 > ArrayCount(APP.gpu.ui.indices))
    return;
  if (APP.gpu.ui.shapes_count + 6 > ArrayCount(APP.gpu.ui.shapes))
    return;

  U32 clip_i = UI_ActiveClipIndex();

  U32 shape_i = APP.gpu.ui.shapes_count;
  APP.gpu.ui.shapes_count += 1;
  APP.gpu.ui.shapes[shape_i] = shape;

  U32 index_i = APP.gpu.ui.indices_count;
  APP.gpu.ui.indices_count += 6;
  U32 encoded = ((shape_i << 2) | (clip_i << 18));
  APP.gpu.ui.indices[index_i + 0] = 0 | encoded;
  APP.gpu.ui.indices[index_i + 1] = 1 | encoded;
  APP.gpu.ui.indices[index_i + 2] = 2 | encoded;
  APP.gpu.ui.indices[index_i + 3] = 2 | encoded;
  APP.gpu.ui.indices[index_i + 4] = 1 | encoded;
  APP.gpu.ui.indices[index_i + 5] = 3 | encoded;
}

static void UI_DrawRect(UI_Shape shape)
{
  shape.tex_layer = -1.f;
  UI_DrawRaw(shape);
}

static void UI_PostFrame()
{
  // ui
  APP.gpu.ui.indices_count = 0;
  APP.gpu.ui.shapes_count = 0;
  APP.gpu.ui.clips[0] = (UI_Clip){0,0,FLT_MAX,FLT_MAX};
  APP.gpu.ui.clips_count = 1;
  APP.gpu.ui.clip_stack[0] = 0;
  APP.gpu.ui.clip_stack_index = 0;
}
