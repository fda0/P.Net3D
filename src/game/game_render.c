static void Rdr_AddRigid(Rdr_RigidType type, Mat4 transform, V4 color)
{
  Assert(type < RdrRigid_COUNT);

  Rdr_Rigid *model = APP.rdr.rigids + type;
  if (model->instance_count < ArrayCount(model->instances))
  {
    Rdr_RigidInstance *inst = model->instances + model->instance_count;
    model->instance_count += 1;
    *inst = (Rdr_RigidInstance)
    {
      .transform = transform,
      .color = color,
    };
  }
}
static void Rdr_AddSkinned(Rdr_SkinnedType type, Mat4 transform, V4 color)
{
  Assert(type < RdrSkinned_COUNT);

  Rdr_Skinned *model = APP.rdr.skinneds + type;
  if (model->instance_count < ArrayCount(model->instances))
  {
    U32 inst_index = model->instance_count;
    model->instance_count += 1;

    Rdr_SkinnedInstance *inst = model->instances + inst_index;
    Rdr_SkinnedPose *pose = model->poses + inst_index;

    *inst = (Rdr_SkinnedInstance)
    {
      .transform = transform,
      .color = color,
      .pose_offset = inst_index*62,
    };

    AN_Pose animation_pose = AN_PoseFromAnimation(&Worker_Skeleton, inst_index*3, APP.at);
    if (animation_pose.matrices_count > ArrayCount(pose->mats))
    {
      Assert(false);
      animation_pose.matrices_count = ArrayCount(pose->mats);
    }
    memcpy(pose->mats, animation_pose.matrices, sizeof(Mat4)*animation_pose.matrices_count);
  }
}
