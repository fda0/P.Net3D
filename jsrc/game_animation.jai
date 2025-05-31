ANIM_AnimateObjects :: ()
{
  for *obj: G.obj.all_objects
  {
    if OBJ_HasAllFlags(obj, .ANIMATE_ROTATION)
    {
      ROTATION_SPEED :: 16.0;

      q0 := obj.l.animated_rot;
      q1 := obj.s.rotation;

      w1 := Min(1.0, G.dt * ROTATION_SPEED);
      w0 := 1.0 - w1;

      if dot(q0, q1) < 0.0
        w1 = -w1;

      obj.l.animated_rot = normalize(Mix(q0, q1, w0, w1));
    }

    if OBJ_HasAnyFlag(obj, .ANIMATE_POSITION)
    {
      POSITION_SPEED :: 10.0;

      delta := obj.s.p - obj.l.animated_p;
      move_by := delta * (G.dt * POSITION_SPEED);
      obj.l.animated_p += move_by;

      for *obj.l.animated_p.component
      {
        if abs(delta.component[it_index]) < 0.01
          it.* = obj.s.p.component[it_index];
      }
    }

    if OBJ_HasAnyFlag(obj, .ANIMATE_TRACKS)
    {
      model := ASSET_GetModel(obj.s.model);
      if model.is_skinned
      {
        WALK_T_SPEED :: 0.016;

        obj.l.animation_index = ASSET_AnimNameToIndex(model.skeleton, "Walk_Loop");
        dist := length(obj.s.moved_dp);
        obj.l.animation_t += dist * WALK_T_SPEED * TICK_RATE; // @todo make this TICK_RATE independent & smooth across small and big TICK_RATEs
        obj.l.animation_t = ANIM_WrapAnimationTime(model.skeleton, obj.l.animation_index, obj.l.animation_t);
      }
    }
  }
}

ANIM_GetPoseTransforms :: (skeleton: ASSET_Skeleton, anim_index: u32, time: float) -> [] Mat4
{
  poses_result := NewArray(skeleton.joints_count, Mat4,, temp);

  // mix default rest pose + animation channels using temporary memory
  {
    scratch := ScopeScratch();

    translations := array_copy(skeleton.bind_translations,, scratch);
    rotations    := array_copy(skeleton.bind_rotations,,    scratch);
    scales       := array_copy(skeleton.bind_scales,,       scratch);

    // Overwrite translations, rotations, scales with values from animation
    if anim_index < skeleton.animations.count
    {
      anim := *skeleton.animations[anim_index];
      time = clamp(anim.t_min, anim.t_max, time);

      for channel: anim.channels
      {
        if channel.joint_index >= skeleton.joints_count
        {
          assert(false);
          continue;
        }

        t := 1.0;
        sample_start: u32;
        sample_end: u32;
        // find t, sample_start, sample_end
        {
          // @speed binary search might be faster?
          for channel.inputs
          {
            if it >= time
              break;
            sample_start = xx it_index;
          }

          sample_end = sample_start + 1;
          if (sample_end >= channel.inputs.count)
            sample_end = sample_start;

          time_start := channel.inputs[sample_start];
          time_end := channel.inputs[sample_end];

          if time_start < time_end
          {
            time_range := time_end - time_start;
            t = (time - time_start) / time_range;
          }
        }

        if channel.type == .Rotation
        {
          v0 := (cast,force([] Quat) channel.outputs)[sample_start];
          v1 := (cast,force([] Quat) channel.outputs)[sample_end];
          value := slerp(v0, v1, t); // @todo NLerp with "neighborhood operator" could be used here?

          rotations[channel.joint_index] = value;
        }
        else
        {
          v0 := (cast,force([] V3) channel.outputs)[sample_start];
          v1 := (cast,force([] V3) channel.outputs)[sample_end];
          value := lerp(v0, v1, t);

          if channel.type == .Translation
            translations[channel.joint_index] = value;
          else
            scales[channel.joint_index] = value;
        }
      }
    }

    for *poses_result
    {
      scale := ScaleMatrix(scales[it_index]); // @optimization skip scale matrices if they are always 1,1,1 ?
      rot := RotationMatrix(rotations[it_index]);
      trans := TranslationMatrix(translations[it_index]);
      it.* = trans * (rot * scale);
    }
  }

  ANIM_WaterfallTransformsToChildren(skeleton, poses_result, 0, skeleton.root_transform);

  for *poses_result
    it.* = it.* * skeleton.inverse_matrices[it_index];

  return poses_result;
}

ANIM_WrapAnimationTime :: (skeleton: ASSET_Skeleton, anim_index: u32, time: float) -> float
{
  if anim_index < skeleton.animations.count
  {
    anim := skeleton.animations[anim_index];
    time = WrapFloat(anim.t_min, anim.t_max, time);
  }
  else
  {
    time = 0;
  }
  return time;
}

ANIM_WaterfallTransformsToChildren :: (skeleton: ASSET_Skeleton, mats: [] Mat4, joint_index: u32, parent_transform: Mat4)
{
  if (joint_index >= skeleton.joints_count)
  {
    assert(false);
    return;
  }

  mats[joint_index] = parent_transform * mats[joint_index];

  child_range := skeleton.child_index_ranges[joint_index];
  for child_range.min..child_range.max-1
  {
    if it >= skeleton.joints_count
    {
      assert(false);
      return;
    }

    child_joint_index := skeleton.child_index_buffer[it];
    ANIM_WaterfallTransformsToChildren(skeleton, mats, child_joint_index, mats[joint_index]);
  }
}
