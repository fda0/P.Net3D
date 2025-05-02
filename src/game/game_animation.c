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
  if (animation_index < skeleton->animations_count)
  {
    AN_Animation *anim = skeleton->animations + animation_index;
    time = FWrap(anim->t_min, anim->t_max, time);
  }
  else
  {
    time = 0;
  }
  return time;
}

static AN_Pose AN_PoseFromAnimation(AN_Skeleton *skeleton, U32 animation_index, float time)
{
  AN_Pose res = {};
  res.mats = Alloc(APP.a_frame, Mat4, skeleton->joints_count);
  res.mats_count = skeleton->joints_count;

  // mix default rest pose + animation channels using temporary memory
  {
    ArenaScope scratch = Arena_PushScope(APP.tmp);

    V3 *translations = Alloc(scratch.a, V3, skeleton->joints_count);
    memcpy(translations, skeleton->translations, sizeof(*translations)*skeleton->joints_count);
    Quat *rotations = Alloc(scratch.a, Quat, skeleton->joints_count);
    memcpy(rotations, skeleton->rotations, sizeof(*rotations)*skeleton->joints_count);
    V3 *scales = Alloc(scratch.a, V3, skeleton->joints_count);
    memcpy(scales, skeleton->scales, sizeof(*scales)*skeleton->joints_count);

    // Overwrite translations, rotations, scales with values from animation
    if (animation_index < skeleton->animations_count)
    {
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
    }

    ForU32(i, res.mats_count)
    {
      Mat4 scale = Mat4_Scale(scales[i]); // @optimization skip scale matrices if they are always 1,1,1 ?
      Mat4 rot = Mat4_Rotation_Quat(rotations[i]);
      Mat4 trans = Mat4_Translation(translations[i]);

      Mat4 combined = Mat4_Mul(rot, scale);
      combined = Mat4_Mul(trans, combined);
      res.mats[i] = combined;
    }

    Arena_PopScope(scratch);
  }

  AN_WaterfallTransformsToChildren(skeleton, res.mats, 0, skeleton->root_transform);

  ForU32(i, res.mats_count)
    res.mats[i] = Mat4_Mul(res.mats[i], skeleton->inv_bind_mats[i]);

  return res;
}
