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
    Rdr_SkinnedInstance *inst = model->instances + model->instance_count;
    model->instance_count += 1;
    *inst = (Rdr_SkinnedInstance)
    {
      .transform = transform,
      .color = color,
    };
  }
}
