static void MDL_Draw(MDL_Kind model_kind, Mat4 transform, U32 color,
                     U32 animation_index, float animation_t)
{
  Assert(model_kind < MDL_COUNT);

  MDL_Batch *batch = APP.gpu.model.batches + model_kind;
  if (batch->instances_count < ArrayCount(batch->instances))
  {
    U32 instance_index = batch->instances_count;
    batch->instances_count += 1;

    MDL_GpuInstance *instance = batch->instances + instance_index;
    *instance = (MDL_GpuInstance)
    {
      .transform = transform,
      .color = color,
    };

    if (MDL_IsSkinned(model_kind))
    {
      AN_Pose animation_pose = AN_PoseFromAnimation(&Worker_Skeleton, animation_index, animation_t);
      U32 poses_left = ArrayCount(APP.gpu.model.poses) - APP.gpu.model.poses_count;

      if (animation_pose.matrices_count <= poses_left)
      {
        instance->pose_offset = APP.gpu.model.poses_count;
        APP.gpu.model.poses_count += animation_pose.matrices_count;

        Mat4 *poses = APP.gpu.model.poses + instance->pose_offset;
        memcpy(poses, animation_pose.matrices, sizeof(Mat4)*animation_pose.matrices_count);
      }
    }
  }
}

static MSH_GpuVertex *MSH_PushVertices(TEX_Kind tex, U32 push_vertices_count)
{
  AssertBounds(tex, APP.gpu.mesh.batches);
  MSH_Batch *batch = APP.gpu.mesh.batches + tex;

  if (batch->vertices_count + push_vertices_count > ArrayCount(batch->vertices))
    return 0;

  MSH_GpuVertex *res = batch->vertices + batch->vertices_count;
  batch->vertices_count += push_vertices_count;
  return res;
}

static U32 UI_ActiveClipIndex()
{
  AssertBounds(APP.gpu.ui.clip_stack_index, APP.gpu.ui.clip_stack);
  U32 clip_index = APP.gpu.ui.clip_stack[APP.gpu.ui.clip_stack_index];
  AssertBounds(clip_index, APP.gpu.ui.clips);
  return clip_index;
}

static UI_GpuClip UI_ActiveClip()
{
  return APP.gpu.ui.clips[UI_ActiveClipIndex()];
}

static void UI_PushClip(UI_GpuClip clip)
{
  if (APP.gpu.ui.clip_stack_index + 1 >= ArrayCount(APP.gpu.ui.clip_stack))
    return;
  if (APP.gpu.ui.clips_count >= ArrayCount(APP.gpu.ui.clips))
    return;

  UI_GpuClip current = UI_ActiveClip();
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

static void UI_DrawRaw(UI_GpuShape shape)
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

static void UI_DrawRect(UI_GpuShape shape)
{
  shape.tex_layer = -1.f;
  UI_DrawRaw(shape);
}

static void GPU_ClearBuffers()
{
  // model
  APP.gpu.model.poses_count = 0;
  ForArray(i, APP.gpu.model.batches)
    APP.gpu.model.batches[i].instances_count = 0;

  // mesh
  ForArray(i, APP.gpu.mesh.batches)
    APP.gpu.mesh.batches[i].vertices_count = 0;

  // ui
  APP.gpu.ui.indices_count = 0;
  APP.gpu.ui.shapes_count = 0;
  APP.gpu.ui.clips[0] = (UI_GpuClip){0,0,FLT_MAX,FLT_MAX};
  APP.gpu.ui.clips_count = 1;
  APP.gpu.ui.clip_stack[0] = 0;
  APP.gpu.ui.clip_stack_index = 0;
}
