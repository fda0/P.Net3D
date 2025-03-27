static void MDL_Add(MDL_Kind model_kind, Mat4 transform, U32 color,
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

static void GPU_PostFrameCleanup()
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
  APP.gpu.ui.clips_count = 0;
}
