static void AN_WaterfallTransformsToChildren(AN_Skeleton *skeleton, Mat4 *mats, U32 joint_index, Mat4 parent_transform)
{
  if (joint_index >= skeleton->joints_count)
  {
    Assert(false);
    return;
  }

  mats[joint_index] = Mat4_Mul(parent_transform, mats[joint_index]);

  RngU32 child_range = skeleton->child_index_ranges[joint_index];
  for (U32 child_index = child_range.min;
       child_index < child_range.max;
       child_index += 1)
  {
    if (child_index >= skeleton->joints_count)
    {
      Assert(false);
      return;
    }

    U32 child_joint_index = skeleton->child_index_buf[child_index];
    AN_WaterfallTransformsToChildren(skeleton, mats, child_joint_index, mats[joint_index]);
  }
}

static float AN_WrapAnimationTime(AN_Skeleton *skeleton, U32 animation_index, float time)
{
  if (animation_index > skeleton->animations_count)
  {
    Assert(false);
    return time;
  }

  AN_Animation *anim = skeleton->animations + animation_index;
  time = WrapF(anim->t_min, anim->t_max, time);
  return time;
}

static AN_Pose AN_PoseFromAnimation(AN_Skeleton *skeleton, U32 animation_index, float time)
{
  AN_Pose res = {};
  res.matrices = Alloc(APP.a_frame, Mat4, skeleton->joints_count);
  res.matrices_count = skeleton->joints_count;

  if (animation_index > skeleton->animations_count)
  {
    Assert(false);
    ForU32(i, res.matrices_count)
      res.matrices[i] = Mat4_Identity();
    return res;
  }

  // mix default rest pose + animation channels using temporary memory
  {
    Arena *a = APP.tmp;
    ArenaScope arena_scope = Arena_PushScope(a);

    V3 *translations = Alloc(a, V3, skeleton->joints_count);
    memcpy(translations, skeleton->translations, sizeof(*translations)*skeleton->joints_count);
    Quat *rotations = Alloc(a, Quat, skeleton->joints_count);
    memcpy(rotations, skeleton->rotations, sizeof(*rotations)*skeleton->joints_count);
    V3 *scales = Alloc(a, V3, skeleton->joints_count);
    memcpy(scales, skeleton->scales, sizeof(*scales)*skeleton->joints_count);

    // Overwrite translations, rotations, scales with values from animation
    AN_Animation *anim = skeleton->animations + animation_index;
    time = Clamp(anim->t_min, anim->t_max, time);

    ForU32(channel_index, anim->channels_count)
    {
      AN_Channel *channel = anim->channels + channel_index;
      if (channel->joint_index >= skeleton->joints_count)
      {
        Assert(false);
        continue;
      }

      float t = 1.f;
      U32 sample_start = 0;
      U32 sample_end = 0;
      // find t, sample_start, sample_end
      {
        // @speed binary search might be faster?
        ForU32(input_index, channel->count)
        {
          if (channel->inputs[input_index] >= time)
            break;
          sample_start = input_index;
        }

        sample_end = sample_start + 1;
        if (sample_end >= channel->count)
          sample_end = sample_start;

        float time_start = channel->inputs[sample_start];
        float time_end = channel->inputs[sample_end];

        if (time_start < time_end)
        {
          float time_range = time_end - time_start;
          t = (time - time_start) / time_range;
        }
      }

      if (channel->type == AN_Rotation)
      {
        Quat v0 = ((Quat *)channel->outputs)[sample_start];
        Quat v1 = ((Quat *)channel->outputs)[sample_end];
        Quat value = Quat_SLerp(v0, v1, t); // @todo NLerp with "neighborhood operator" could be used here?

        rotations[channel->joint_index] = value;
      }
      else
      {
        V3 v0 = ((V3 *)channel->outputs)[sample_start];
        V3 v1 = ((V3 *)channel->outputs)[sample_end];
        V3 value = V3_Lerp(v0, v1, t);

        if (channel->type == AN_Translation)
          translations[channel->joint_index] = value;
        else
          scales[channel->joint_index] = value;
      }
    }

    ForU32(i, res.matrices_count)
    {
      Mat4 scale = Mat4_Scale(scales[i]); // @optimization skip scale matrices if they are always 1,1,1 ?
      Mat4 rot = Mat4_Rotation_Quat(rotations[i]);
      Mat4 trans = Mat4_Translation(translations[i]);

      Mat4 combined = Mat4_Mul(rot, scale);
      combined = Mat4_Mul(trans, combined);
      res.matrices[i] = combined;
    }

    Arena_PopScope(arena_scope);
  }

  float scale = 40.f;
  Mat4 root_transform = Mat4_Scale((V3){scale, scale, scale});
  root_transform = Mat4_Mul(Mat4_Rotation_RH((V3){1,0,0}, 0.25f), root_transform);
  root_transform = Mat4_Mul(Mat4_Rotation_RH((V3){0,0,1}, 0.25f), root_transform);
  AN_WaterfallTransformsToChildren(skeleton, res.matrices, 0, root_transform);

  ForU32(i, res.matrices_count)
    res.matrices[i] = Mat4_Mul(res.matrices[i], skeleton->inverse_bind_matrices[i]);

  return res;
}
