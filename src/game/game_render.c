static void RDR_AddRigid(RDR_RigidType type, Mat4 transform, U32 color)
{
  Assert(type < RdrRigid_COUNT);

  RDR_Rigid *model = APP.rdr.rigids + type;
  if (model->instance_count < ArrayCount(model->instances))
  {
    RDR_RigidInstance *inst = model->instances + model->instance_count;
    model->instance_count += 1;
    *inst = (RDR_RigidInstance)
    {
      .transform = transform,
      .color = color,
    };
  }
}
static void RDR_AddSkinned(RDR_SkinnedType type, Mat4 transform, U32 color,
                           U32 animation_index, float animation_t)
{
  Assert(type < RdrSkinned_COUNT);

  RDR_Skinned *model = APP.rdr.skinneds + type;
  if (model->instance_count < ArrayCount(model->instances))
  {
    U32 inst_index = model->instance_count;
    model->instance_count += 1;

    RDR_SkinnedInstance *inst = model->instances + inst_index;
    RDR_SkinnedPose *pose = model->poses + inst_index;

    *inst = (RDR_SkinnedInstance)
    {
      .transform = transform,
      .color = color,
      .pose_offset = inst_index*62,
    };

    AN_Pose animation_pose = AN_PoseFromAnimation(&Worker_Skeleton, animation_index, animation_t);
    if (animation_pose.matrices_count > ArrayCount(pose->mats))
    {
      Assert(false);
      animation_pose.matrices_count = ArrayCount(pose->mats);
    }
    memcpy(pose->mats, animation_pose.matrices, sizeof(Mat4)*animation_pose.matrices_count);
  }
}

static RDR_WallVertex *RDR_PushWallVertices(TEX_Kind tex, U32 push_vert_count)
{
  AssertBounds(tex, APP.rdr.wall_mesh_buffers);
  RDR_WallMeshBuffer *buf = APP.rdr.wall_mesh_buffers + tex;
  if (buf->vert_count + push_vert_count > ArrayCount(buf->verts))
    return 0;

  RDR_WallVertex *res = buf->verts + buf->vert_count;
  buf->vert_count += push_vert_count;
  return res;
}

static void RDR_PostFrameCleanup()
{
  ForArray(i, APP.gpu.rigids)
    APP.rdr.rigids[i].instance_count = 0;

  ForArray(i, APP.gpu.skinneds)
    APP.rdr.skinneds[i].instance_count = 0;


  ForArray(buffer_i, APP.rdr.wall_mesh_buffers)
  {
    RDR_WallMeshBuffer *buf = APP.rdr.wall_mesh_buffers + buffer_i;
    buf->vert_count = 0;
  }
}