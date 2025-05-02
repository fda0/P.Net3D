static void *M_FakeMalloc(void *user_data, U64 size)
{
  (void)user_data;
  U64 default_align = 16;
  return Arena_AllocateBytes(BAKER.cgltf_arena, size, default_align, false);
}
static void M_FakeFree(void *user_data, void *ptr)
{
  (void)user_data;
  (void)ptr;
  // do nothing
}

static U64 BK_GLTF_FindJointIndex(cgltf_skin *skin, cgltf_node *find_node)
{
  U64 result = skin->joints_count;
  ForU64(joint_index, skin->joints_count)
  {
    if (find_node == skin->joints[joint_index])
    {
      result = joint_index;
      break;
    }
  }
  return result;
}


static void BK_GLTF_ExportSkeletonToBread(BREAD_Builder *bb, BK_GLTF_ModelData *model, cgltf_data *data)
{
  if (model->has_skeleton)
    return;

  model->has_skeleton = true;
  model->skeleton_index = bb->skeletons_count;
  bb->skeletons_count += 1;

  BREAD_Skeleton *br_skel = BREAD_Reserve(&bb->skeletons, BREAD_Skeleton, 1);

  // Root transform
  {
    BREAD_RangeStart(&bb->file, &br_skel->root_transform);
    Mat4 root_transform = Mat4_Scale((V3){model->config.scale, model->config.scale, model->config.scale});
    root_transform = Mat4_Mul(Mat4_Rotation_Quat(model->config.rot), root_transform);
    *BREAD_Reserve(&bb->file, Mat4, 1) = root_transform;
    BREAD_RangeEnd(&bb->file, &br_skel->root_transform, 1);
  }

  M_Check(data->skins_count == 1);
  cgltf_skin *skin = data->skins;
  const U32 joints_count = (U32)skin->joints_count;

  // Joints data
  {
    // unpack inverse bind matrices
    {
      cgltf_accessor *inv_bind = skin->inverse_bind_matrices;
      M_Check(inv_bind);
      M_Check(inv_bind->count == joints_count);
      M_Check(inv_bind->component_type == cgltf_component_type_r_32f);
      M_Check(inv_bind->type == cgltf_type_mat4);

      U64 comp_count = cgltf_num_components(inv_bind->type);
      U64 number_count = comp_count * inv_bind->count;

      float *numbers = BREAD_ReserveToRange(&bb->file, &br_skel->inv_bind_mats, float, number_count);
      U64 unpacked = cgltf_accessor_unpack_floats(inv_bind, numbers, number_count);
      M_Check(unpacked == number_count);
    }

    // child hierarchy indices
    {
      U32 *indices = BREAD_ReserveToRange(&bb->file, &br_skel->child_index_buf, U32, joints_count);
      RngU32 *ranges = BREAD_ReserveToRange(&bb->file, &br_skel->child_index_ranges, RngU32, joints_count);

      U32 indices_count = 0;
      ForU64(joint_index, joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];

        ForU64(child_index, joint->children_count)
        {
          cgltf_node *child = joint->children[child_index];
          U64 child_joint_index = BK_GLTF_FindJointIndex(skin, child);

          if (child_index == 0)
            ranges[joint_index].min = indices_count;

          M_Check(indices_count < joints_count);
          indices[indices_count] = child_joint_index;
          indices_count += 1;

          ranges[joint_index].max = indices_count;
        }
      }
    }

    // rest pose transformations
    {
      // Translations
      BREAD_RangeStart(&bb->file, &br_skel->translations);
      ForU64(joint_index, joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];
        ForArray(i, joint->translation)
          *BREAD_Reserve(&bb->file, float, 1) = joint->translation[i];
      }
      BREAD_RangeEnd(&bb->file, &br_skel->translations, joints_count);
      M_Check(br_skel->translations.size == br_skel->translations.elem_count*sizeof(V3));

      // Rotations
      BREAD_RangeStart(&bb->file, &br_skel->rotations);
      ForU64(joint_index, joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];
        ForArray(i, joint->rotation)
          *BREAD_Reserve(&bb->file, float, 1) = joint->rotation[i];
      }
      BREAD_RangeEnd(&bb->file, &br_skel->rotations, joints_count);
      M_Check(br_skel->rotations.size == br_skel->rotations.elem_count*sizeof(Quat));

      // Scales
      BREAD_RangeStart(&bb->file, &br_skel->scales);
      ForU64(joint_index, joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];
        ForArray(i, joint->scale)
          *BREAD_Reserve(&bb->file, float, 1) = joint->scale[i];
      }
      BREAD_RangeEnd(&bb->file, &br_skel->scales, joints_count);
      M_Check(br_skel->scales.size == br_skel->scales.elem_count*sizeof(V3));
    }

    // First allocate all strings in one continuous chunk.
    // Track min&max paris of each string.
    RngU32 *name_ranges = AllocZeroed(BAKER.tmp, RngU32, joints_count);
    ForU64(joint_index, joints_count)
    {
      cgltf_node *joint = skin->joints[joint_index];
      name_ranges[joint_index].min = bb->file.used;
      Pr_Cstr(&bb->file, joint->name);
      name_ranges[joint_index].max = bb->file.used;
    }

    // Ensure memory alignment after copying strings
    BREAD_Aling(&bb->file, _Alignof(RngU32));

    // Memcpy name strign ranges into the file
    {
      RngU32 *dst_name_ranges = BREAD_ReserveToRange(&bb->file, &br_skel->name_ranges, RngU32, joints_count);
      Memcpy(dst_name_ranges, name_ranges, br_skel->name_ranges.size);
    }
  }

  // Animations
  if (data->animations_count)
  {
    U32 anims_count = (U32)data->animations_count;
    BREAD_Animation *br_animations = BREAD_ReserveToRange(&bb->file, &br_skel->animations, BREAD_Animation, anims_count);

    ForU32(animation_index, anims_count)
    {
      BREAD_Animation *br_anim = br_animations + animation_index;
      cgltf_animation *gltf_anim = data->animations + animation_index;
      float t_min = FLT_MAX;
      float t_max = -FLT_MAX;

      // Name string
      S8 anim_name_string = S8_FromCstr(gltf_anim->name);
      BREAD_RangeStart(&bb->file, &br_anim->name_string);
      Pr_S8(&bb->file, anim_name_string);
      BREAD_RangeEnd(&bb->file, &br_anim->name_string, anim_name_string.size);

      // Ensure memory alignment after copying strings
      BREAD_Aling(&bb->file, _Alignof(BREAD_AnimChannel));

      // Channels
      U32 channels_count = (U32)gltf_anim->channels_count;
      BREAD_AnimChannel *br_channels = BREAD_ReserveToRange(&bb->file, &br_anim->channels,
                                                            BREAD_AnimChannel, channels_count);

      ForU32(channel_index, channels_count)
      {
        BREAD_AnimChannel *br_chan = br_channels + channel_index;
        cgltf_animation_channel *gltf_chan = gltf_anim->channels + channel_index;
        cgltf_animation_sampler *gltf_sampler = gltf_chan->sampler;
        M_Check(gltf_sampler->interpolation == cgltf_interpolation_type_linear);
        M_Check(gltf_sampler->input->count == gltf_sampler->output->count);

        U32 sample_count = (U32)gltf_sampler->input->count;
        br_chan->joint_index = BK_GLTF_FindJointIndex(skin, gltf_chan->target_node);

        switch (gltf_chan->target_path)
        {
          default:
          M_Check(false && "Unsupported channel target"); break;

          case cgltf_animation_path_type_translation:
          br_chan->type = AN_Translation; break;

          case cgltf_animation_path_type_rotation:
          br_chan->type = AN_Rotation; break;

          case cgltf_animation_path_type_scale:
          br_chan->type = AN_Scale; break;
        }

        // inputs
        U32 in_count = sample_count;
        float *in_nums = BREAD_ReserveToRange(&bb->file, &br_chan->inputs, float, in_count);
        U64 in_unpacked = cgltf_accessor_unpack_floats(gltf_sampler->input, in_nums, in_count);
        M_Check(in_unpacked == in_count);

        ForU32(i, in_count)
        {
          float n = in_nums[i];
          if (n > t_max) t_max = n;
          if (n < t_min) t_min = n;
        }

        // outputs
        U32 out_comp_count = (U32)cgltf_num_components(gltf_sampler->output->type);
        U32 out_count = out_comp_count * sample_count;
        float *out_nums = BREAD_ReserveToRange(&bb->file, &br_chan->outputs, float, out_count);
        U64 out_unpacked = cgltf_accessor_unpack_floats(gltf_sampler->output, out_nums, out_count);
        M_Check(out_unpacked == out_count);
      }

      br_anim->t_min = t_min;
      br_anim->t_max = t_max;
    }
  }
}


static void BK_GLTF_ExportSkeletonToPrinter(Printer *p, BK_GLTF_ModelData *model, cgltf_data *data)
{
  M_Check(data->skins_count == 1);
  cgltf_skin *skin = data->skins;

  Pr_S8(p, S8Lit("static AN_Skeleton "));
  Pr_S8(p, model->name);
  Pr_S8(p, S8Lit("_Skeleton = {\n"));

  {
    Mat4 root_transform = Mat4_Scale((V3){model->config.scale, model->config.scale, model->config.scale});
    root_transform = Mat4_Mul(Mat4_Rotation_Quat(model->config.rot), root_transform);
    Pr_S8(p, S8Lit(".root_transform = {\n"));
    Pr_FloatArray(p, root_transform.flat, ArrayCount(root_transform.flat));
    Pr_S8(p, S8Lit("\n},\n"));
  }

  // anims[] + anims_count
  {
    if (data->animations_count)
    {
      Pr_S8(p, S8Lit(".animations =(AN_Animation []) {\n"));

      ForU64(animation_index, data->animations_count)
      {
        cgltf_animation *animation = data->animations + animation_index;

        // Animation
        float t_min = FLT_MAX;
        float t_max = -FLT_MAX;

        Pr_S8(p, S8Lit("// animation: "));
        Pr_U64(p, animation_index);
        Pr_S8(p, S8Lit("\n"));
        Pr_S8(p, S8Lit("{\n"));

        Pr_S8(p, S8Lit(".name = \""));
        Pr_Cstr(p, animation->name);
        Pr_S8(p, S8Lit("\",\n"));

        Pr_S8(p, S8Lit(".channels = (AN_Channel[]) {\n"));
        ForU64(channel_index, animation->channels_count)
        {
          cgltf_animation_channel *channel = animation->channels + channel_index;
          cgltf_animation_sampler *sampler = channel->sampler;
          M_Check(sampler->interpolation == cgltf_interpolation_type_linear);
          M_Check(sampler->input->count == sampler->output->count);
          U64 sample_count = sampler->input->count;

          Pr_S8(p, S8Lit("// channel: "));
          Pr_U64(p, channel_index);
          Pr_S8(p, S8Lit(" (animation "));
          Pr_U64(p, animation_index);
          Pr_S8(p, S8Lit(")\n"));
          Pr_S8(p, S8Lit("{\n"));

          U64 target_joint_index = BK_GLTF_FindJointIndex(skin, channel->target_node);
          Pr_S8(p, S8Lit(".joint_index = "));
          Pr_U64(p, target_joint_index);
          Pr_S8(p, S8Lit(",\n"));

          Pr_S8(p, S8Lit(".type = AN_"));
          if (channel->target_path == cgltf_animation_path_type_translation) Pr_S8(p, S8Lit("Translation"));
          if (channel->target_path == cgltf_animation_path_type_rotation)    Pr_S8(p, S8Lit("Rotation"));
          if (channel->target_path == cgltf_animation_path_type_scale)       Pr_S8(p, S8Lit("Scale"));
          Pr_S8(p, S8Lit(",\n"));

          Pr_S8(p, S8Lit(".count = "));
          Pr_U64(p, sampler->input->count);
          Pr_S8(p, S8Lit(",\n"));

          // inputs
          {
            Pr_S8(p, S8Lit(".inputs = (float[]) {\n"));

            U64 number_count = sample_count;

            float *numbers = Alloc(BAKER.tmp, float, number_count);
            U64 unpacked = cgltf_accessor_unpack_floats(sampler->input, numbers, number_count);
            M_Check(unpacked == number_count);
            Pr_FloatArray(p, numbers, unpacked);

            // update t_min, t_max
            ForU64(i, unpacked)
            {
              float n = numbers[i];
              if (n > t_max) t_max = n;
              if (n < t_min) t_min = n;
            }

            Pr_S8(p, S8Lit("\n},\n"));
          }

          // outputs
          {
            Pr_S8(p, S8Lit(".outputs = (float[]) {\n"));

            U64 comp_count = cgltf_num_components(sampler->output->type);
            U64 number_count = sample_count * comp_count;

            float *numbers = Alloc(BAKER.tmp, float, number_count);
            U64 unpacked = cgltf_accessor_unpack_floats(sampler->output, numbers, number_count);
            M_Check(unpacked == number_count);
            Pr_FloatArray(p, numbers, unpacked);

            Pr_S8(p, S8Lit("\n},\n"));
          }

          Pr_S8(p, S8Lit("},\n"));
        }
        Pr_S8(p, S8Lit("},\n"));

        Pr_S8(p, S8Lit(".channels_count = "));
        Pr_U64(p, animation->channels_count);
        Pr_S8(p, S8Lit(",\n"));

        Pr_S8(p, S8Lit(".t_min = "));
        Pr_Float(p, t_min);
        Pr_S8(p, S8Lit("f,\n"));

        Pr_S8(p, S8Lit(".t_max = "));
        Pr_Float(p, t_max);
        Pr_S8(p, S8Lit("f,\n"));

        Pr_S8(p, S8Lit("},\n"));
      }

      Pr_S8(p, S8Lit("},\n"));
    }

    Pr_S8(p, S8Lit(".animations_count = "));
    Pr_U64(p, data->animations_count);
    Pr_S8(p, S8Lit(",\n"));
  }

  // joints section
  {
    Pr_S8(p, S8Lit(".joints_count = "));
    Pr_U64(p, skin->joints_count);
    Pr_S8(p, S8Lit(",\n"));

    Pr_S8(p, S8Lit(".names = (const char *[]){\n"));
    ForU64(joint_index, skin->joints_count)
    {
      cgltf_node *joint = skin->joints[joint_index];
      Pr_S8(p, S8Lit("  \""));
      Pr_Cstr(p, joint->name);
      Pr_S8(p, S8Lit("\",\n"));
    }
    Pr_S8(p, S8Lit("},\n"));

    // unpack inverse bind matrices
    {
      Pr_S8(p, S8Lit(".inverse_bind_matrices = (Mat4[]){\n"));

      cgltf_accessor *inv_bind = skin->inverse_bind_matrices;
      M_Check(inv_bind);
      M_Check(inv_bind->count == skin->joints_count);
      M_Check(inv_bind->component_type == cgltf_component_type_r_32f);
      M_Check(inv_bind->type == cgltf_type_mat4);

      U64 comp_count = cgltf_num_components(inv_bind->type);
      U64 number_count = comp_count * inv_bind->count;

      float *numbers = Alloc(BAKER.tmp, float, number_count);
      U64 unpacked = cgltf_accessor_unpack_floats(inv_bind, numbers, number_count);
      M_Check(unpacked == number_count);
      Pr_FloatArray(p, numbers, number_count);

      Pr_S8(p, S8Lit("\n},\n"));
    }

    // child hierarchy indices
    {
      U32 *indices = AllocZeroed(BAKER.tmp, U32, skin->joints_count);
      U32 indices_count = 0;
      RngU32 *ranges = AllocZeroed(BAKER.tmp, RngU32, skin->joints_count);

      ForU64(joint_index, skin->joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];

        ForU64(child_index, joint->children_count)
        {
          cgltf_node *child = joint->children[child_index];
          U64 child_joint_index = BK_GLTF_FindJointIndex(skin, child);

          if (child_index == 0)
            ranges[joint_index].min = indices_count;

          M_Check(indices_count < skin->joints_count);
          indices[indices_count] = child_joint_index;
          indices_count += 1;

          ranges[joint_index].max = indices_count;
        }
      }

      // index buf
      Pr_S8(p, S8Lit(".child_index_buf = (U32[]){\n"));
      ForU64(i, skin->joints_count)
      {
        Pr_U32(p, indices[i]);
        Pr_S8(p, S8Lit(","));
      }
      Pr_S8(p, S8Lit("\n},\n"));

      // index ranges
      Pr_S8(p, S8Lit(".child_index_ranges = (RngU32[]){\n"));
      ForU64(i, skin->joints_count)
      {
        Pr_S8(p, S8Lit("{"));
        Pr_U32(p, ranges[i].min);
        Pr_S8(p, S8Lit(","));
        Pr_U32(p, ranges[i].max);
        Pr_S8(p, S8Lit("},"));
      }
      Pr_S8(p, S8Lit("\n},\n"));
    }

    // rest pose transformations
    {
      // unpack rest pose transforms
      {
        Pr_S8(p, S8Lit(".translations = (V3[]){"));
        ForU64(joint_index, skin->joints_count)
        {
          cgltf_node *joint = skin->joints[joint_index];
          ForArray(i, joint->translation)
          {
            Pr_Float(p, joint->translation[i]);
            Pr_S8(p, S8Lit("f,"));
          }
          Pr_S8(p, S8Lit(" "));
        }
        Pr_S8(p, S8Lit("},\n"));

        Pr_S8(p, S8Lit(".rotations = (Quat[]){"));
        ForU64(joint_index, skin->joints_count)
        {
          cgltf_node *joint = skin->joints[joint_index];
          ForArray(i, joint->rotation)
          {
            Pr_Float(p, joint->rotation[i]);
            Pr_S8(p, S8Lit("f,"));
          }
          Pr_S8(p, S8Lit(" "));
        }
        Pr_S8(p, S8Lit("},\n"));

        Pr_S8(p, S8Lit(".scales = (V3[]){"));
        ForU64(joint_index, skin->joints_count)
        {
          cgltf_node *joint = skin->joints[joint_index];
          ForArray(i, joint->scale)
          {
            Pr_Float(p, joint->scale[i]);
            Pr_S8(p, S8Lit("f,"));
          }
          Pr_S8(p, S8Lit(" "));
        }
        Pr_S8(p, S8Lit("},\n"));
      }
    }
  }

  Pr_S8(p, S8Lit("};\n\n\n"));
}

static void *BK_GLTF_UnpackAccessor(cgltf_accessor *accessor, BK_Buffer *buffer)
{
  U64 comp_count = cgltf_num_components(accessor->type);
  U64 total_count = accessor->count * comp_count;
  U64 comp_size = cgltf_component_size(accessor->component_type);

  if (accessor->component_type == cgltf_component_type_r_32f)
  {
    M_Check(buffer->elem_size == 4);
    float *numbers = BK_BufferPushFloat(buffer, total_count);
    U64 unpacked = cgltf_accessor_unpack_floats(accessor, numbers, total_count);
    M_Check(unpacked == total_count);
    return numbers;
  }

  void *numbers = BK_BufferPush(buffer, total_count);

  if (buffer->elem_size >= comp_size)
  {
    U64 unpacked = cgltf_accessor_unpack_indices(accessor, numbers, buffer->elem_size, total_count);
    M_Check(unpacked == total_count);
  }
  else
  {
    // Alloc tmp buffer
    ArenaScope scope = Arena_PushScope(BAKER.tmp);
    U64 bytes_to_allocate = comp_size * total_count;
    void *tmp_nums = Arena_AllocateBytes(scope.a, bytes_to_allocate, comp_size, false);

    // Copy big type into tmp buffer
    U64 unpacked = cgltf_accessor_unpack_indices(accessor, tmp_nums, comp_size, total_count);
    M_Check(unpacked == total_count);

    // Copy into numbers with checked truncation
    ForU64(i, total_count)
    {
      // Load value from tmp_nums into U64
      U64 value = 0;
      if      (comp_size == 8) value = ((U64 *)tmp_nums)[i];
      else if (comp_size == 4) value = ((U32 *)tmp_nums)[i];
      else if (comp_size == 2) value = ((U16 *)tmp_nums)[i];
      else if (comp_size == 1) value = ((U8 *)tmp_nums)[i];
      else M_Check(!"Unsupported comp_size downconversion");

      // Store into buffer backed array with truncation
      if (buffer->elem_size == 1)
      {
        M_Check(value <= U8_MAX);
        ((U8 *)numbers)[i] = (U8)value;
      }
      else if (buffer->elem_size == 2)
      {
        M_Check(value <= U16_MAX);
        ((U16 *)numbers)[i] = (U16)value;
      }
      else if (buffer->elem_size == 4)
      {
        M_Check(value <= U32_MAX);
        ((U32 *)numbers)[i] = (U32)value;
      }
      else
        M_Check(!"Unsupported elem_size downconversion");
    }

    Arena_PopScope(scope);
  }

  return numbers;
}

static void BK_GLTF_ExportModelToPrinter(Printer *out, BK_GLTF_ModelData *model)
{
  Pr_S8(out, S8Lit("static "));
  Pr_S8(out, model->is_skinned ? S8Lit("MDL_GpuSkinnedVertex") : S8Lit("MDL_GpuRigidVertex"));
  Pr_S8(out, S8Lit(" Model_"));
  Pr_S8(out, model->name);
  Pr_S8(out, S8Lit("_vrt[] =\n{\n"));

  ForU64(vert_i, model->verts_count)
  {
    V3 normal =
    {
      *BK_BufferAtFloat(&model->normals, vert_i*3 + 0),
      *BK_BufferAtFloat(&model->normals, vert_i*3 + 1),
      *BK_BufferAtFloat(&model->normals, vert_i*3 + 2),
    };
    float normal_lensq = V3_LengthSq(normal);
    if (normal_lensq < 0.001f)
    {
      M_LOG(M_LogGltfWarning, "[GLTF LOADER] Found normal equal to zero");
      normal = (V3){0,0,1};
    }
    else if (normal_lensq < 0.9f || normal_lensq > 1.1f)
    {
      M_LOG(M_LogGltfWarning, "[GLTF LOADER] Normal wasn't normalized");
      normal = V3_Scale(normal, FInvSqrt(normal_lensq));
    }
    Quat normal_rot = Quat_FromZupCrossV3(normal);
    Pr_S8(out, S8Lit("/*nrm*/"));
    Pr_Float(out, normal_rot.x);
    Pr_S8(out, S8Lit("f,"));
    Pr_Float(out, normal_rot.y);
    Pr_S8(out, S8Lit("f,"));
    Pr_Float(out, normal_rot.z);
    Pr_S8(out, S8Lit("f,"));
    Pr_Float(out, normal_rot.w);
    Pr_S8(out, S8Lit("f, "));

    Pr_S8(out, S8Lit("  /*pos*/"));
    Pr_Float(out, *BK_BufferAtFloat(&model->positions, vert_i*3 + 0));
    Pr_S8(out, S8Lit("f,"));
    Pr_Float(out, *BK_BufferAtFloat(&model->positions, vert_i*3 + 1));
    Pr_S8(out, S8Lit("f,"));
    Pr_Float(out, *BK_BufferAtFloat(&model->positions, vert_i*3 + 2));
    Pr_S8(out, S8Lit("f, "));

    Pr_S8(out, S8Lit("/*col*/0x"));
    Pr_U32Hex(out, *BK_BufferAtU32(&model->colors, vert_i));
    Pr_S8(out, S8Lit(", "));

    if (model->is_skinned)
    {
      U32 joint_index_vals[4] =
      {
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 0),
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 1),
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 2),
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 3),
      };
      ForArray(i, joint_index_vals)
      {
        if (joint_index_vals[i] >= model->joints_count)
          M_LOG(M_LogGltfWarning, "[GLTF LOADER] Joint index overflow. %u >= %llu",
                joint_index_vals[i], model->joints_count);
      }

      U32 joint_packed4 = (joint_index_vals[0] | (joint_index_vals[1] << 8) |
                           (joint_index_vals[2] << 16) | (joint_index_vals[3] << 24));

      Pr_S8(out, S8Lit("/*jnt*/0x"));
      Pr_U32Hex(out, joint_packed4);
      Pr_S8(out, S8Lit(", "));

      float w0 = *BK_BufferAtFloat(&model->weights, vert_i*4 + 0);
      float w1 = *BK_BufferAtFloat(&model->weights, vert_i*4 + 1);
      float w2 = *BK_BufferAtFloat(&model->weights, vert_i*4 + 2);
      float w3 = *BK_BufferAtFloat(&model->weights, vert_i*4 + 3);
      float weight_sum = w0 + w1 + w2 + w3;
      if (weight_sum < 0.9f || weight_sum > 1.1f)
      {
        M_LOG(M_LogGltfWarning, "[GLTF LOADER] Weight sum == %f (should be 1)", weight_sum);
      }

      Pr_S8(out, S8Lit("/*wgt*/"));
      Pr_Float(out, w0);
      Pr_S8(out, S8Lit("f,"));
      Pr_Float(out, w1);
      Pr_S8(out, S8Lit("f,"));
      Pr_Float(out, w2);
      Pr_S8(out, S8Lit("f,"));
      Pr_Float(out, w3);
      Pr_S8(out, S8Lit("f,"));
    }

    Pr_S8(out, S8Lit("\n"));
  }
  Pr_S8(out, S8Lit("};\n\n"));


  Pr_S8(out, S8Lit("static U16 Model_"));
  Pr_S8(out, model->name);
  Pr_S8(out, S8Lit("_ind[] =\n{"));
  ForU64(i, model->indices.used)
  {
    U64 per_group = 3;
    U64 per_row = per_group * 6;
    if (i % per_row == 0)
      Pr_S8(out, S8Lit("\n  "));
    else if ((i % per_row) % per_group == 0)
      Pr_S8(out, S8Lit(" "));

    Pr_U16(out, *BK_BufferAtU16(&model->indices, i));
    Pr_S8(out, S8Lit(","));
  }
  Pr_S8(out, S8Lit("\n};\n\n"));
}

static void BK_GLTF_ExportModelToBread(BREAD_Builder *bb, BK_GLTF_ModelData *model)
{
  BREAD_AddModel(bb, model->type, model->is_skinned);

  // vertices
  ForU64(vert_i, model->verts_count)
  {
    V3 normal =
    {
      *BK_BufferAtFloat(&model->normals, vert_i*3 + 0),
      *BK_BufferAtFloat(&model->normals, vert_i*3 + 1),
      *BK_BufferAtFloat(&model->normals, vert_i*3 + 2),
    };
    float normal_lensq = V3_LengthSq(normal);
    if (normal_lensq < 0.001f)
    {
      M_LOG(M_LogGltfWarning, "[GLTF LOADER] Found normal equal to zero");
      normal = (V3){0,0,1};
    }
    else if (normal_lensq < 0.9f || normal_lensq > 1.1f)
    {
      M_LOG(M_LogGltfWarning, "[GLTF LOADER] Normal wasn't normalized");
      normal = V3_Scale(normal, FInvSqrt(normal_lensq));
    }
    Quat normal_rot = Quat_FromZupCrossV3(normal);

    V3 pos = {};
    pos.x = *BK_BufferAtFloat(&model->positions, vert_i*3 + 0);
    pos.y = *BK_BufferAtFloat(&model->positions, vert_i*3 + 1);
    pos.z = *BK_BufferAtFloat(&model->positions, vert_i*3 + 2);

    U32 color = *BK_BufferAtU32(&model->colors, vert_i);

    if (!model->is_skinned) // it's rigid
    {
      WORLD_GpuRigidVertex rigid = {};
      rigid.normal_rot = normal_rot;
      rigid.p = pos;
      rigid.color = color;
      *BREAD_AddModelRigidVertex(bb) = rigid;
    }
    else // it's skinned
    {
      U32 joint_indices[4] =
      {
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 0),
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 1),
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 2),
        *BK_BufferAtU8(&model->joint_indices, vert_i*4 + 3),
      };
      ForArray(i, joint_indices)
      {
        if (joint_indices[i] >= model->joints_count)
          M_LOG(M_LogGltfWarning, "[GLTF LOADER] Joint index overflow. %u >= %llu",
                joint_indices[i], model->joints_count);
      }
      U32 joints_packed4 = (joint_indices[0] |
                            (joint_indices[1] << 8) |
                            (joint_indices[2] << 16) |
                            (joint_indices[3] << 24));

      V4 weights = {};
      weights.x = *BK_BufferAtFloat(&model->weights, vert_i*4 + 0);
      weights.y = *BK_BufferAtFloat(&model->weights, vert_i*4 + 1);
      weights.z = *BK_BufferAtFloat(&model->weights, vert_i*4 + 2);
      weights.w = *BK_BufferAtFloat(&model->weights, vert_i*4 + 3);
      float weight_sum = weights.x + weights.y + weights.z + weights.w;
      if (weight_sum < 0.9f || weight_sum > 1.1f)
        M_LOG(M_LogGltfWarning, "[GLTF LOADER] Weight sum == %f (should be 1)", weight_sum);

      WORLD_GpuSkinnedVertex skinned = {};
      skinned.normal_rot = normal_rot;
      skinned.p = pos;
      skinned.color = color;
      skinned.joints_packed4 = joints_packed4;
      skinned.weights = weights;
      *BREAD_AddModelSkinnedVertex(bb) = skinned;
    }
  }

  BREAD_CopyIndices(bb, model->indices.vals, model->indices.used);
}

static void BK_GLTF_Load(MODEL_Type model_type, const char *name, const char *path,
                         Printer *out, Printer *out_a, BREAD_Builder *bb, BK_GLTF_ModelConfig config)
{
  // normalize config
  if (!config.scale) config.scale = 1.f;
  if (Memeq(&config.rot, &(Quat){}, sizeof(Quat))) config.rot = Quat_Identity();

  //
  // Parse .gltf file using cgltf library
  //
  Arena_Reset(BAKER.cgltf_arena, 0);
  cgltf_options options =
  {
    .memory =
    {
      .alloc_func = M_FakeMalloc,
      .free_func = M_FakeFree,
    },
  };
  cgltf_data *data = 0;
  cgltf_result res = cgltf_parse_file(&options, path, &data);
  if (res != cgltf_result_success)
  {
    M_LOG(M_LogErr, "[GLTF LOADER] Failed to parse %s", path);
    return;
  }

  res = cgltf_load_buffers(&options, data, path);
  if (res != cgltf_result_success)
  {
    M_LOG(M_LogErr, "[GLTF LOADER] Failed to load buffers, %s", path);
    return;
  }

  res = cgltf_validate(data);
  if (res != cgltf_result_success)
  {
    M_LOG(M_LogErr, "[GLTF LOADER] Failed to validate data, %s", path);
    return;
  }

  //
  // Collect data from .gltf file
  //
  ArenaScope scratch = Arena_PushScope(BAKER.tmp);
  U64 max_indices = 1024*256;
  U64 max_verts = 1024*128;

  BK_GLTF_ModelData model = {};
  model.config = config;
  model.type = model_type;
  model.indices = BK_BufferAlloc(scratch.a, max_indices, sizeof(U16));
  model.positions     = BK_BufferAlloc(scratch.a, max_verts*3, sizeof(float));
  model.normals       = BK_BufferAlloc(scratch.a, max_verts*3, sizeof(float));
  model.texcoords     = BK_BufferAlloc(scratch.a, max_verts*2, sizeof(float));
  model.joint_indices = BK_BufferAlloc(scratch.a, max_verts*4, sizeof(U8));
  model.weights       = BK_BufferAlloc(scratch.a, max_verts*4, sizeof(float));
  model.colors        = BK_BufferAlloc(scratch.a, max_verts*1, sizeof(U32)); // this doesn't need such a big array actually

  ForU64(mesh_index, data->meshes_count)
  {
    cgltf_mesh *mesh = data->meshes + mesh_index;

    ForU64(primitive_index, mesh->primitives_count)
    {
      cgltf_primitive *primitive = mesh->primitives + primitive_index;

      if (primitive->type != cgltf_primitive_type_triangles)
      {
        M_LOG(M_LogGltfWarning,
              "[GLTF LOADER] Primitive with non triangle type (%u) skipped",
              primitive->type);
        continue;
      }

      M_Check(primitive->indices);
      {
        U64 index_start_offset = model.positions.used/3;

        cgltf_accessor *accessor = primitive->indices;
        M_Check(accessor->type == cgltf_type_scalar);
        U16 *numbers = BK_GLTF_UnpackAccessor(accessor, &model.indices);

        if (index_start_offset)
        {
          ForU64(i, accessor->count)
          {
            U32 value = numbers[i] + index_start_offset;
            M_Check(value <= U16_MAX);
            numbers[i] = value;
          }
        }
      }

      ForU64(attribute_index, primitive->attributes_count)
      {
        cgltf_attribute *attribute = primitive->attributes + attribute_index;
        cgltf_accessor *accessor = attribute->data;

        U64 comp_count = cgltf_num_components(accessor->type);
        BK_Buffer *save_buf = 0;

        switch (attribute->type)
        {
          case cgltf_attribute_type_position:
          {
            save_buf = &model.positions;
            M_Check(comp_count == 3);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_normal:
          {
            save_buf = &model.normals;
            M_Check(comp_count == 3);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_tangent:
          {
            M_LOG(M_LogGltfDebug, "[GLTF LOADER] Attribute with type tangent skipped");
            continue;
          } break;

          case cgltf_attribute_type_texcoord:
          {
            save_buf = &model.texcoords;
            M_Check(comp_count == 2);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_joints:
          {
            save_buf = &model.joint_indices;
            M_Check(comp_count == 4);
            //M_Check(accessor->component_type == cgltf_component_type_r_8u);
          } break;

          case cgltf_attribute_type_weights:
          {
            save_buf = &model.weights;
            M_Check(comp_count == 4);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          default:
          {
            M_LOG(M_LogGltfWarning,
                  "[GLTF LOADER] Attribute with unknown type (%u) skipped",
                  attribute->type);
            continue;
          } break;
        }

        BK_GLTF_UnpackAccessor(accessor, save_buf);

        // @todo this is a temporary hack that saves a color per position
        if (attribute->type == cgltf_attribute_type_position)
        {
          ForU64(vert_i, accessor->count)
          {
            cgltf_material* material = primitive->material;
            V4 material_color = *(V4 *)material->pbr_metallic_roughness.base_color_factor;
            static_assert(sizeof(material->pbr_metallic_roughness.base_color_factor) == sizeof(material_color));

            U32 *color_value = BK_BufferPushU32(&model.colors, 1);
            *color_value = Color32_V4(material_color);
          }
        }
      }
    }
  }

  //
  //
  //
  model.name = S8_FromCstr(name);
  if (!model.name.size)
    model.name = M_NameFromPath(S8_FromCstr(path));

  model.is_skinned = (data->animations_count > 0);

  // Validate buffer counts
  M_Check(model.positions.used % 3 == 0);
  model.verts_count = model.positions.used / 3;

  M_Check(model.normals.used % 3 == 0);
  M_Check(model.verts_count == model.normals.used / 3);

  if (model.is_skinned)
  {
    M_Check(model.joint_indices.used % 4 == 0);
    M_Check(model.verts_count == model.joint_indices.used / 4);
    M_Check(model.weights.used % 4 == 0);
    M_Check(model.verts_count == model.weights.used / 4);

    // Joints
    M_Check(data->skins_count == 1);
    M_Check(data->skins[0].joints_count <= U32_MAX);
    model.joints_count = (U32)data->skins[0].joints_count;
  }

  // Sekeleton export
  if (model.is_skinned)
  {
    BK_GLTF_ExportSkeletonToPrinter(out_a, &model, data);
    BK_GLTF_ExportSkeletonToBread(bb, &model, data);
  }

  if (!model.is_skinned) // apply transformations directly to rigid mesh
  {
    if (config.scale != 1.f || !Quat_IsIdentity(config.rot) ||
        config.move.x || config.move.y || config.move.z)
    {
      ForU32(i, model.verts_count)
      {
        V3 *position = (V3*)BK_BufferAtFloat(&model.positions, i*3);
        V3 p = *position;
        p = V3_Scale(p, config.scale);
        p = V3_Rotate(p, config.rot);
        p = V3_Add(p, config.move);

        V3 *normal = (V3*)BK_BufferAtFloat(&model.normals, i*3);
        V3 n = *normal;
        n = V3_Rotate(n, config.rot);

        *position = p;
        *normal = n;
      }
    }
  }

  // Vertices export
  if (false)
    BK_GLTF_ExportModelToPrinter(out, &model);
  else
    BK_GLTF_ExportModelToBread(bb, &model);

  // Reset tmp arena
  Arena_PopScope(scratch);

  M_Check(!out->err);
  M_Check(!out_a->err);
}
