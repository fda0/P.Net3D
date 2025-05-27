ANIM_TransformType :: PIE_TransformType;

ANIM_Channel :: struct
{
  type: ANIM_TransformType;
  joint_index: u32;
  inputs: [] float;
  outputs: [] float; // .count = inputs.count * (ifx type == .Rotation then 4 else 3);
};

ANIM_Animation :: struct
{
  name: string;
  t_min, t_max: float;
  channels: [] ANIM_Channel;
};

ANIM_Skeleton :: struct
{
  root_transform: Mat4;
  animations: [] ANIM_Animation;

  joints_count: u32; // all arrays below have count equal to joints_count
  joint_names: [] string;
  inverse_matrices: [] Mat4; // inverse bind matrices
  child_index_buffer: [] u32;
  child_index_ranges: [] Range(u32);

  bind_translations: [] V3;
  bind_rotations: [] Quat;
  bind_scales: [] V3;
};
