ANIMATION_AdvanceMode :: enum
{
  TIME;
  DISTANCE;
  MANUAL;
}

ANIMATION_Record :: struct
{
  animation_index: s32;
  joint_weights: [] float; // Animations like punching shouldn't really apply to legs etc
}

ANIMATION_RecordType :: enum u8
{
  IDLE;
  WALK;
  RUN;
  PUNCH;
  HIT;
  DIE;
}

// ANIMATION_Request :: struct
// {
//   start: TimestampMS;
//   record: ANIMATION_RecordType;
//   mode: enum { DISABLED; ONCE; LOOP; };
// }

ANIMATION_PoseParams :: struct
{
  type: ANIMATION_RecordType;
  t: float;
  weight: float;
}

ANIMATION_State :: struct
{
  arena: Arena;
  records: [#run EnumCount(ANIMATION_RecordType)] ANIMATION_Record;
}

ANIMATION_TypeToRecord :: (type: ANIMATION_RecordType) -> ANIMATION_Record
{
  return G.anim.records[type];
}

ANIMATION_Init :: ()
{
  using G.anim;

  skeleton := ASSET_GetModel(MODEL_CreateKey("Dude")).skeleton;
  if skeleton
  {
    for enum_values_as_enum(ANIMATION_RecordType)
    {
      using records[it];
      joint_weights = NewArray(skeleton.joints_count, float,, arena);
      for *joint_weights it.* = 1.0;

      if it == {
        case .WALK;
        animation_index = AnimationNameToIndex(skeleton, "Walk_Loop");

        case .PUNCH;
        animation_index = AnimationNameToIndex(skeleton, "Punch_Cross");
        joint_weights[JointNameToIndex(skeleton, "DEF-thigh.L")] = 0;
        joint_weights[JointNameToIndex(skeleton, "DEF-thigh.R")] = 0;

        case .RUN;
        animation_index = AnimationNameToIndex(skeleton, "Jog_Fwd_Loop");
        joint_weights[JointNameToIndex(skeleton, "DEF-thigh.R")] = 0;
      }

      ANIMATION_WaterfallToChildren(skeleton, 0, joint_weights, 1.0);
    }
  }
}

ANIMATION_AnimateObjects :: ()
{
  for *obj: G.obj.all_objects
  {
    if OBJ_HasAllFlags(obj, .ANIMATE_ROTATION)
    {
      ROTATION_SPEED :: 16.0;

      q0 := obj.l.animated_rot;
      q1 := obj.s.rotation;

      w1 := min(1.0, G.dt * ROTATION_SPEED);
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

        anim_mode: ANIMATION_AdvanceMode = .DISTANCE;

        for *obj.l.animations
        {
          if it_index == 0
          {
            it.weight = 1.0;
            it.type = .WALK;

            if obj.s.is_attacking && !obj.s.hide_attack_animation_tail
            {
              attack_normalized_t := WrapFloat(0.0, ATTACK_COOLDOWN_T, obj.s.attack_t);
              attack_normalized_t /= ATTACK_COOLDOWN_T;

              anim_mode = .MANUAL;
              it.type = .PUNCH;
              it.t = lerp(0.0, 1.0, attack_normalized_t);
            }

            // if IndexInBounds(model.skeleton.animations, obj.l.overwrite_animation_index)
            // {
            //   it.index = obj.l.overwrite_animation_index;
            //   anim_mode = obj.l.overwrite_animation_mode;
            // }
          }
          else if it_index == 1
          {
            it.type = .RUN;
            it.weight = 1.0;
          }
          else continue;

          // Advance T
          if anim_mode == .DISTANCE
          {
            dist := length(obj.s.moved_dp);
            it.t += dist * WALK_T_SPEED * TICK_RATE; // @todo make this TICK_RATE independent & smooth across small and big TICK_RATEs
          }
          else if anim_mode == .TIME
          {
            it.t += G.dt;
          }

          // Wrap T
          it.t = ANIMATION_WrapAnimationTime(model.skeleton, it.type, it.t);
        }


      }
    }
  }
}

ANIMATION_GetPoseTransforms :: (skeleton: ASSET_Skeleton, params: [] ANIMATION_PoseParams) -> [] Mat4
{
  accumulated_scales       := array_copy(skeleton.bind_scales,,       temp);
  accumulated_rotations    := array_copy(skeleton.bind_rotations,,    temp);
  accumulated_translations := array_copy(skeleton.bind_translations,, temp);

  // mix default rest pose + animation channels using temporary memory
  for param, param_index: params
  {
    if param.weight <= 0 continue;

    auto_release_temp();
    scales: [] V3;
    rotations: [] Quat;
    translations: [] V3;

    if param_index == 0
    {
      scales = accumulated_scales;
      rotations = accumulated_rotations;
      translations = accumulated_translations;
    }
    else
    {
      scales       = array_copy(skeleton.bind_scales,,       temp);
      rotations    = array_copy(skeleton.bind_rotations,,    temp);
      translations = array_copy(skeleton.bind_translations,, temp);
    }

    record := ANIMATION_TypeToRecord(param.type);

    // Overwrite translations, rotations, scales with values from animation
    {
      anim := *skeleton.animations[record.animation_index];
      time := clamp(param.t, anim.t_min, anim.t_max);

      for channel: anim.channels
      {
        if channel.joint_index >= skeleton.joints_count
        {
          assert(false);
          continue;
        }

        if record.joint_weights[channel.joint_index] <= 0.0 then continue;

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
          q0 := channel.outputs.data.(*Quat)[sample_start];
          q1 := channel.outputs.data.(*Quat)[sample_end];
          value := Slerp(q0, q1, t); // @todo NLerp with "neighborhood operator" could be used here?

          rotations[channel.joint_index] = value;
        }
        else
        {
          v0 := channel.outputs.data.(*V3)[sample_start];
          v1 := channel.outputs.data.(*V3)[sample_end];
          value := lerp(v0, v1, t);

          if channel.type == .Translation
            translations[channel.joint_index] = value;
          else
            scales[channel.joint_index] = value;
        }
      }
    }

    if param_index > 0
    {
      for joint_weight: record.joint_weights
      {
        weight := param.weight * joint_weight;
        accumulated_scales[it_index]       = lerp(accumulated_scales[it_index],       scales[it_index],       weight);
        accumulated_rotations[it_index]    = Slerp(accumulated_rotations[it_index],   rotations[it_index],    weight);
        accumulated_translations[it_index] = lerp(accumulated_translations[it_index], translations[it_index], weight);
      }
    }
  }

  result_matrices := NewArray(skeleton.joints_count, Mat4,, temp);
  for *result_matrices
  {
    scale := ScaleMatrix(accumulated_scales[it_index]);
    rot := RotationMatrix(accumulated_rotations[it_index]);
    trans := TranslationMatrix(accumulated_translations[it_index]);
    it.* = trans * (rot * scale);
  }

  ANIMATION_WaterfallToChildren(skeleton, 0, result_matrices, skeleton.root_transform);

  for *result_matrices
    it.* = it.* * skeleton.inverse_matrices[it_index];

  return result_matrices;
}

ANIMATION_WrapAnimationTime :: (skeleton: ASSET_Skeleton, anim_index: s32, time: float) -> float
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
ANIMATION_WrapAnimationTime :: (skeleton: ASSET_Skeleton, type: ANIMATION_RecordType, time: float) -> float
{
  return ANIMATION_WrapAnimationTime(skeleton, G.anim.records[type].animation_index, time);
}

ANIMATION_WaterfallToChildren :: (skeleton: ASSET_Skeleton, joint_index: u32, values: [] $T, parent_value: T)
{
  // This function traverses skeleton joint hierarchy from root joint to children.
  // It propagates values using multiplication along the way.

  if (joint_index >= skeleton.joints_count)
  {
    assert(false);
    return;
  }

  values[joint_index] = parent_value * values[joint_index];

  child_range := skeleton.child_index_ranges[joint_index];
  for child_range
  {
    if it >= skeleton.joints_count
    {
      assert(false);
      return;
    }

    child_joint_index := skeleton.child_index_buffer[it];
    ANIMATION_WaterfallToChildren(skeleton, child_joint_index, values, values[joint_index]);
  }
}
