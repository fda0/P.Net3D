static void AN_WaterfallTransformsToChildren(Mat4 *mats, U64 mat_count, Mat4 parent_transform)
{
  M_AssertAlways(joint_index < mat_count);
  mats[joint_index] = Mat4_Mul(parent_transform, mats[joint_index]);

  M_AssertAlways(joint_index < skin->joints_count);
  cgltf_node *joint = skin->joints[joint_index];

  ForU64(child_index, joint->children_count)
  {
    U64 child_index = child_index;

    cgltf_node *child = joint->children[child_index];
    U64 child_joint_index = M_FindJointIndex(skin, child);
    M_AssertAlways(child_joint_index < mat_count);
    M_AssertAlways(child_joint_index > joint_index);

    M_WaterfallTransformsToChildren(skin, child_joint_index, mats, mat_count, mats[joint_index], depth + 1);
  }
}

static AN_SkeletonTransform AN_SkeletonTransformFromAnimation(AN_Skeleton *skeleton, U32 animation_index, float time)
{
  AN_SkeletonTransform res = {};
  res.matrices = Alloc(APP.a_frame, Mat4, skeleton->joints_count);
  res.matrices_count = skeleton->joints_count;

  if (animation_index > skeleton->animations_count)
  {
    Assert(false);
    ForU32(i, res.matrices_count)
      res.matrices[i] = Mat4_Identity();
    return res;
  }

  float t = WrapF(skeleton->t_min, skeleton->t_max, time);

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
    ForU32(channel_index, anim->channels_count)
    {
      AN_Channel *channel = anim->channels + channel_index;
      if (channel->joint_index >= skeleton->joints_count)
      {
        Assert(false);
        continue;
      }

      // @todo scan channel->inputs and select proper start_index, end_index, interpolate between them
      U32 sample_index = 0;

      if (channel->type == AN_Rotation)
      {
        Quat value = ((Quat *)channel->outputs)[sample_index];
        rotations[channel->joint_index] = value;
      }
      else
      {
        V3 value = ((V3 *)channel->outputs)[sample_index];

        if (channel->type == AN_Translation)
          translations[channel->joint_index] = value;
        else
          scales[channel->joint_index] = value;
      }
    }

    ForU32(i, res.matrices_count)
    {
      Mat4 scale = Mat4_Scale(scales[i]);
      Mat4 rot = Mat4_Rotation_Quat(rotations[i]);
      Mat4 trans = Mat4_Translation(translations[i]);

      Mat4 combined = Mat4_Mul(rot, scale);
      combined = Mat4_Mul(trans, combined);
      res.matrices[i] = combined;
    }

    Arena_PopScope(arena_scope);
  }

  // @todo waterfall these transformations

  // @todo multiply by inverse bind matrices

  return res;
}
