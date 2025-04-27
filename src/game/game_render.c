static void MDL_Draw(MDL_Kind model_kind, Mat4 transform, U32 color,
                     U32 animation_index, float animation_t)
{
  MDL_GpuInstance instance =
  {
    .transform = transform,
    .color = color,
  };

  if (MDL_IsSkinned(model_kind))
  {
    AN_Pose animation_pose = AN_PoseFromAnimation(&Worker_Skeleton, animation_index, animation_t);
    GPU_MemoryResult anim_mem =
      GPU_MemoryAlloc((GPU_MemorySpec){.target = GPU_MemoryJointTransforms,
                                       .count = animation_pose.matrices_count});

    Assert(anim_mem.alloc_size == sizeof(Mat4)*animation_pose.matrices_count);
    Memcpy(anim_mem.alloc_mapped, animation_pose.matrices, anim_mem.alloc_size);
    instance.pose_offset = anim_mem.alloc_offset_in_elements;
  }

  GPU_MemoryResult instance_mem =
    GPU_MemoryAlloc((GPU_MemorySpec){.target = GPU_MemoryModelInstances,
                                     .model = model_kind,
                                     .count = 1});
  Assert(instance_mem.alloc_size == sizeof(instance));
  Memcpy(instance_mem.alloc_mapped, &instance, instance_mem.alloc_size);
}

static void MSH_DrawVertices(TEX_Kind tex, MSH_GpuVertex *vertices, U32 vertices_count)
{
  GPU_MemoryResult vertices_mem =
    GPU_MemoryAlloc((GPU_MemorySpec){.target = GPU_MemoryMeshVertices,
                                     .tex = tex,
                                     .count = vertices_count});
  Assert(vertices_mem.alloc_size == sizeof(vertices[0])*vertices_count);
  Memcpy(vertices_mem.alloc_mapped, vertices, vertices_mem.alloc_size);
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
