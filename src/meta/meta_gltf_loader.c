static void *M_FakeMalloc(void *user_data, U64 size)
{
  (void)user_data;
  U64 default_align = 16;
  return Arena_AllocateBytes(M.cgltf_arena, size, default_align, false);
}
static void M_FakeFree(void *user_data, void *ptr)
{
  (void)user_data;
  (void)ptr;
  // do nothing
}

static U64 M_FindJointIndex(cgltf_skin *skin, cgltf_node *find_node)
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

static void M_ExtractRestPose(cgltf_skin *skin, U64 joint_index,
                              Mat4 *mats, U64 mat_count,
                              Mat4 parent_mat,
                              I32 depth)
{
  M_AssertAlways(joint_index < skin->joints_count);
  cgltf_node *joint = skin->joints[joint_index];
  M_AssertAlways(!joint->has_matrix);

  // logging
  {
    Pr_MakeOnStack(p, 512);
    ForI32(i, depth)
      Pr_Add(&p, S8Lit("  "));

    Pr_Add(&p, S8Lit("("));
    Pr_AddU32(&p, depth);
    Pr_Add(&p, S8Lit(")"));
    Pr_AddCstr(&p, joint->name);

    S8 p_s8 = Pr_S8(&p);
    printf("%.*s\n", S8Print(p_s8));
  }


  Mat4 transform_mat = Mat4_Identity();
  if (joint->has_scale)
  {
    V3 scale = *(V3 *)joint->scale;
    static_assert(sizeof(joint->scale) == sizeof(scale));
    transform_mat = Mat4_Mul(Mat4_Scale(scale), transform_mat);
  }
  if (joint->has_rotation)
  {
    Quat rotation = *(Quat *)joint->rotation;
    static_assert(sizeof(joint->rotation) == sizeof(rotation));
    Mat4 rot_mat = Mat4_Rotation_Quat(rotation);
    transform_mat = Mat4_Mul(rot_mat, transform_mat);
  }
  if (joint->has_translation)
  {
    V3 translation = *(V3 *)joint->translation;
    static_assert(sizeof(joint->translation) == sizeof(translation));
    transform_mat = Mat4_Mul(Mat4_Translation(translation), transform_mat);
  }

  mats[joint_index] = Mat4_Mul(parent_mat, transform_mat);

  ForU64(child_index, joint->children_count)
  {
    cgltf_node *child = joint->children[child_index];
    U64 next_joint_index = M_FindJointIndex(skin, child);
    M_AssertAlways(next_joint_index < mat_count);
    M_AssertAlways(next_joint_index > joint_index);

    //Mat4 parent_mat_for_child = mats[joint_index];
    Mat4 parent_mat_for_child = Mat4_Identity();
    M_ExtractRestPose(skin, next_joint_index, mats, mat_count, parent_mat_for_child, depth + 1);
  }
}


static void M_WaterfallTransformsToChildren(cgltf_skin *skin, U64 joint_index,
                                            Mat4 *mats, U64 mat_count,
                                            Mat4 parent_transform, I32 depth)
{
  M_AssertAlways(joint_index < mat_count);
  mats[joint_index] = Mat4_Mul(parent_transform, mats[joint_index]);

  M_AssertAlways(joint_index < skin->joints_count);
  cgltf_node *joint = skin->joints[joint_index];

  // logging
  {
    U64 parent_joint_index = M_FindJointIndex(skin, joint->parent);

    Pr_MakeOnStack(p, 512);
    ForI32(i, depth)
      Pr_Add(&p, S8Lit("    "));

    Pr_Add(&p, S8Lit("[d "));
    Pr_AddI32(&p, depth);
    Pr_Add(&p, S8Lit("; i "));
    Pr_AddU64(&p, joint_index);
    Pr_Add(&p, S8Lit("; p "));
    Pr_AddU64(&p, parent_joint_index);
    Pr_Add(&p, S8Lit("] "));
    Pr_AddCstr(&p, joint->name);

    S8 p_s8 = Pr_S8(&p);
    printf("%.*s\n", S8Print(p_s8));
  }

  ForU64(child_index_, joint->children_count)
  {
    U64 child_index = child_index_;
    //U64 child_index = joint->children_count - child_index_ - 1;

    cgltf_node *child = joint->children[child_index];
    U64 child_joint_index = M_FindJointIndex(skin, child);
    M_AssertAlways(child_joint_index < mat_count);
    M_AssertAlways(child_joint_index > joint_index);

    M_WaterfallTransformsToChildren(skin, child_joint_index, mats, mat_count, mats[joint_index], depth + 1);
  }
}


static void M_GLTF_Load(const char *path, Printer *out, Printer *out2)
{
  //
  // Parse .gltf file using cgltf library
  //
  Arena_Reset(M.cgltf_arena, 0);
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
  ArenaScope tmp_scope = Arena_PushScope(M.tmp);

  U64 max_indices = 1024*256;
  M_Buffer indices = M_BufferAlloc(M.tmp, max_indices, sizeof(U16));

  U64 max_verts = 1024*128;
  M_Buffer positions     = M_BufferAlloc(M.tmp, max_verts*3, sizeof(float));
  M_Buffer normals       = M_BufferAlloc(M.tmp, max_verts*3, sizeof(float));
  M_Buffer texcoords     = M_BufferAlloc(M.tmp, max_verts*2, sizeof(float));
  M_Buffer joint_indices = M_BufferAlloc(M.tmp, max_verts*4, sizeof(U8));
  M_Buffer weights       = M_BufferAlloc(M.tmp, max_verts*4, sizeof(float));

  U64 max_joints = 1024;
  M_Buffer inv_mats      = M_BufferAlloc(M.tmp, max_joints*16, sizeof(float));
  M_Buffer pose_mats     = M_BufferAlloc(M.tmp, max_joints*16, sizeof(float));
  M_Buffer inv_mat_names = M_BufferAlloc(M.tmp, max_joints,    sizeof(S8));
  M_Buffer scale_anims   = M_BufferAlloc(M.tmp, max_joints*16, sizeof(float));
  M_Buffer rotate_anims  = M_BufferAlloc(M.tmp, max_joints*16, sizeof(float));
  M_Buffer transl_anims  = M_BufferAlloc(M.tmp, max_joints*16, sizeof(float));

  ForU64(mesh_index, data->meshes_count)
  {
    cgltf_mesh *mesh = data->meshes + mesh_index;
    //if (mesh_index != 1)
      //continue;

    ForU64(primitive_index, mesh->primitives_count)
    {
      cgltf_primitive *primitive = mesh->primitives + primitive_index;
      //if (primitive_index != 0)
        //continue;

      if (primitive->type != cgltf_primitive_type_triangles)
      {
        M_LOG(M_LogGltfWarning,
              "[GLTF LOADER] Primitive with non triangle type (%u) skipped",
              primitive->type);
        continue;
      }

      M_AssertAlways(primitive->indices);
      {
        U64 index_start_offset = positions.used/3;

        cgltf_accessor *accessor = primitive->indices;
        M_AssertAlways(accessor->component_type == cgltf_component_type_r_16u);
        M_AssertAlways(accessor->type == cgltf_type_scalar);

        U16 *numbers = M_BufferPushU16(&indices, accessor->count);
        U64 unpacked = cgltf_accessor_unpack_indices(accessor, numbers, indices.elem_size, accessor->count);
        M_AssertAlways(unpacked == accessor->count);

        if (index_start_offset)
        {
          ForU64(i, accessor->count)
            numbers[i] += index_start_offset;
        }
      }

      ForU64(attribute_index, primitive->attributes_count)
      {
        cgltf_attribute *attribute = primitive->attributes + attribute_index;
        cgltf_accessor *accessor = attribute->data;

        U64 comp_count = cgltf_num_components(accessor->type);
        M_Buffer *save_buf = 0;

        switch (attribute->type)
        {
          case cgltf_attribute_type_position:
          {
            save_buf = &positions;
            M_AssertAlways(comp_count == 3);
            M_AssertAlways(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_normal:
          {
            save_buf = &normals;
            M_AssertAlways(comp_count == 3);
            M_AssertAlways(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_texcoord:
          {
            save_buf = &texcoords;
            M_AssertAlways(comp_count == 2);
            M_AssertAlways(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_joints:
          {
            save_buf = &joint_indices;
            M_AssertAlways(comp_count == 4);
            M_AssertAlways(accessor->component_type == cgltf_component_type_r_8u);
          } break;

          case cgltf_attribute_type_weights:
          {
            save_buf = &weights;
            M_AssertAlways(comp_count == 4);
            M_AssertAlways(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          default:
          {
            M_LOG(M_LogGltfWarning,
                  "[GLTF LOADER] Attribute with unknown type (%u) skipped",
                  attribute->type);
            continue;
          } break;
        }

        U64 total_count = comp_count * accessor->count;
        U64 unpacked = 0;
        if (save_buf->elem_size == 4)
        {
          float *numbers = M_BufferPushFloat(save_buf, total_count);
          unpacked = cgltf_accessor_unpack_floats(accessor, numbers, total_count);
          M_AssertAlways(unpacked == total_count);
        }
        else if (save_buf->elem_size == 2)
        {
          U16 *numbers = M_BufferPushU16(save_buf, total_count);
          unpacked = cgltf_accessor_unpack_indices(accessor, numbers, save_buf->elem_size, total_count);
          M_AssertAlways(unpacked == total_count);
        }
        else if (save_buf->elem_size == 1)
        {
          U8 *numbers = M_BufferPushU8(save_buf, total_count);
          unpacked = cgltf_accessor_unpack_indices(accessor, numbers, save_buf->elem_size, total_count);
        }
        M_AssertAlways(unpacked == total_count);
      }
    }
  }

  M_AssertAlways(data->skins_count == 1);
  cgltf_skin *skin = data->skins + 0;
  {
    ForU64(joint_index, skin->joints_count)
    {
      cgltf_node *joint = skin->joints[joint_index];
      *M_BufferPushS8(&inv_mat_names, 1) = S8_ScanCstr(joint->name);
    }

    cgltf_accessor *accessor = skin->inverse_bind_matrices;
    M_AssertAlways(accessor);
    M_AssertAlways(accessor->count == skin->joints_count);
    M_AssertAlways(accessor->component_type == cgltf_component_type_r_32f);
    M_AssertAlways(accessor->type == cgltf_type_mat4);

    U64 comp_count = cgltf_num_components(accessor->type);
    U64 total_count = comp_count * accessor->count;

    float *numbers = M_BufferPushFloat(&inv_mats, total_count);
    U64 unpacked = cgltf_accessor_unpack_floats(accessor, numbers, total_count);
    M_AssertAlways(unpacked == total_count);
  }

  M_BufferPushFloat(&pose_mats, inv_mats.used);
  M_BufferPushFloat(&scale_anims, inv_mats.used);
  M_BufferPushFloat(&rotate_anims, inv_mats.used);
  M_BufferPushFloat(&transl_anims, inv_mats.used);
  Mat4 *poses = (Mat4 *)M_BufferAtFloat(&pose_mats, 0);
  Mat4 *invs  = (Mat4 *)M_BufferAtFloat(&inv_mats, 0);
  Mat4 *s_anims = (Mat4 *)M_BufferAtFloat(&scale_anims, 0);
  Mat4 *r_anims = (Mat4 *)M_BufferAtFloat(&rotate_anims, 0);
  Mat4 *t_anims = (Mat4 *)M_BufferAtFloat(&transl_anims, 0);
  U64 joint_mat_count = inv_mats.used / 16;

  // init poses & anim mats
  ForU64(i, joint_mat_count)
  {
    poses[i] = Mat4_Identity();
    s_anims[i] = Mat4_Identity();
    r_anims[i] = Mat4_Identity();
    t_anims[i] = Mat4_Identity();
  }

  ForU64(animation_index, data->animations_count)
  {
    cgltf_animation *animation = data->animations + animation_index;

    if (animation_index != 14)
      continue;

    ForU64(channel_index_, animation->channels_count)
    {
      //U64 channel_index = animation->channels_count - channel_index_ - 1;
      U64 channel_index = channel_index_;
      cgltf_animation_channel *channel = animation->channels + channel_index;

      U64 joint_index = M_FindJointIndex(skin, channel->target_node);
      M_AssertAlways(joint_index < skin->joints_count);
      M_AssertAlways(joint_index < joint_mat_count);

      cgltf_animation_sampler *sampler = channel->sampler;
      M_AssertAlways(sampler->interpolation == cgltf_interpolation_type_linear);
      M_AssertAlways(sampler->input->count == sampler->output->count);

      //U64 from_index = sampler->input->count / 2;
      //U64 from_index = 0;
      U64 from_index = sampler->input->count - 1;
      float in = 0;
      cgltf_accessor_read_float(sampler->input, from_index, &in, 1);

      U64 comp_count = cgltf_num_components(sampler->output->type);
      float outs[4] = {};
      M_AssertAlways(ArrayCount(outs) >= comp_count);
      cgltf_accessor_read_float(sampler->output, from_index, outs, comp_count);

      char *type = "";
      if (channel->target_path == cgltf_animation_path_type_scale)
      {
        M_AssertAlways(comp_count == 3);
        type = "scale";
        Mat4 *anim_mat = s_anims + joint_index;
        *anim_mat = Mat4_Mul(Mat4_Scale(*(V3 *)outs), *anim_mat);
      }
      else if (channel->target_path == cgltf_animation_path_type_rotation)
      {
        M_AssertAlways(comp_count == 4);
        type = "rotation   ";
        Mat4 *anim_mat = r_anims + joint_index;
        *anim_mat = Mat4_Mul(Mat4_Rotation_Quat(*(Quat *)outs), *anim_mat);
      }
      else if (channel->target_path == cgltf_animation_path_type_translation)
      {
        M_AssertAlways(comp_count == 3);
        type = "translation";
        Mat4 *anim_mat = t_anims + joint_index;
        *anim_mat = Mat4_Mul(Mat4_Translation(*(V3 *)outs), *anim_mat);
      }
      else
      {
        M_AssertAlways(false);
      }


      // logging
      {
        printf("channel_i: %03llu; in time: %f; joint_index: %02llu; %s;\t"
               "%.2f %.2f %.2f %.2f\n",
               channel_index, in, joint_index, type,
               outs[0], outs[1], outs[2], outs[3]);
      }
    }
  }


  M_ExtractRestPose(skin, 0, poses, joint_mat_count, Mat4_Identity(), 0);

#if 1
  ForU64(i, joint_mat_count)
  {
    bool missing_t = Mat4_IsIdentity(t_anims[i]);
    if (missing_t)
    {
      printf("missing translation for %llu\n", i);
      t_anims[i] = Mat4_LeaveOnlyTranslation(poses[i]);
    }

    Mat4 rs = Mat4_Mul(r_anims[i], s_anims[i]);
    t_anims[i] = Mat4_Mul(t_anims[i], rs);
  }

  M_WaterfallTransformsToChildren(skin, 0, t_anims, joint_mat_count, Mat4_Identity(), 0);

  ForU64(i, joint_mat_count)
  {
    poses[i] = t_anims[i];
  }
#endif

  // combine pose and inv matrices
  ForU64(i, joint_mat_count)
  {
    invs[i] = Mat4_Mul(poses[i], invs[i]);
  }

  //
  // Output collected data
  //
  M_AssertAlways(positions.used % 3 == 0);
  U64 positions_count = positions.used / 3;

  M_AssertAlways(normals.used % 3 == 0);
  U64 normals_count = normals.used / 3;

  M_AssertAlways(joint_indices.used % 4 == 0);
  U64 joint_indices_count = joint_indices.used / 4;

  M_AssertAlways(weights.used % 4 == 0);
  U64 weights_count = weights.used / 4;

  M_AssertAlways(positions_count == normals_count);
  M_AssertAlways(positions_count == joint_indices_count);
  M_AssertAlways(positions_count == weights_count);

  Pr_Add(out, S8Lit("static Rdr_SkinnedVertex Model_Worker_vrt[] =\n{\n"));
  ForU64(vert_i, positions_count)
  {
    Pr_Add(out, S8Lit("  /*pos*/"));
    Pr_AddFloat(out, *M_BufferAtFloat(&positions, vert_i*3 + 0));
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, *M_BufferAtFloat(&positions, vert_i*3 + 1));
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, *M_BufferAtFloat(&positions, vert_i*3 + 2));
    Pr_Add(out, S8Lit("f, "));

    Pr_Add(out, S8Lit("/*col*/1.f,1.f,1.f, "));

    V3 normal =
    {
      *M_BufferAtFloat(&normals, vert_i*3 + 0),
      *M_BufferAtFloat(&normals, vert_i*3 + 1),
      *M_BufferAtFloat(&normals, vert_i*3 + 2),
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
      normal = V3_Scale(normal, InvSqrtF(normal_lensq));
    }

    Pr_Add(out, S8Lit("/*nrm*/"));
    Pr_AddFloat(out, normal.x);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, normal.y);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, normal.z);
    Pr_Add(out, S8Lit("f, "));

    U32 joint_index_vals[4] =
    {
      *M_BufferAtU8(&joint_indices, vert_i*4 + 0),
      *M_BufferAtU8(&joint_indices, vert_i*4 + 1),
      *M_BufferAtU8(&joint_indices, vert_i*4 + 2),
      *M_BufferAtU8(&joint_indices, vert_i*4 + 3),
    };
    ForArray32(i, joint_index_vals)
    {
      if (joint_index_vals[i] >= inv_mat_names.used)
        M_LOG(M_LogGltfWarning, "[GLTF LOADER] Joint index overflow. %u >= %u",
              joint_index_vals[i], (U32)inv_mat_names.used);
    }

    U32 joint_packed4 = (joint_index_vals[0] | (joint_index_vals[1] << 8) |
                         (joint_index_vals[2] << 16) | (joint_index_vals[3] << 24));

    Pr_Add(out, S8Lit("/*jnt*/0x"));
    Pr_AddU32Hex(out, joint_packed4);
    Pr_Add(out, S8Lit(", "));

    float w0 = *M_BufferAtFloat(&weights, vert_i*4 + 0);
    float w1 = *M_BufferAtFloat(&weights, vert_i*4 + 1);
    float w2 = *M_BufferAtFloat(&weights, vert_i*4 + 2);
    float w3 = *M_BufferAtFloat(&weights, vert_i*4 + 3);
    float weight_sum = w0 + w1 + w2 + w3;
    if (weight_sum < 0.9f || weight_sum > 1.1f)
    {
      M_LOG(M_LogGltfWarning, "[GLTF LOADER] Weight sum == %f (should be 1)", weight_sum);
    }

    Pr_Add(out, S8Lit("/*wgt*/"));
    Pr_AddFloat(out, w0);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, w1);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, w2);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, w3);
    Pr_Add(out, S8Lit("f,"));

    Pr_Add(out, S8Lit("\n"));
  }
  Pr_Add(out, S8Lit("};\n\n"));


  Pr_Add(out, S8Lit("static U16 Model_Worker_ind[] =\n{"));
  ForU64(i, indices.used)
  {
    U64 per_group = 3;
    U64 per_row = per_group * 6;
    if (i % per_row == 0)
      Pr_Add(out, S8Lit("\n  "));
    else if ((i % per_row) % per_group == 0)
      Pr_Add(out, S8Lit(" "));

    Pr_AddU16(out, *M_BufferAtU16(&indices, i));
    Pr_Add(out, S8Lit(","));
  }
  Pr_Add(out, S8Lit("\n};\n\n"));


  M_AssertAlways(inv_mats.used % 16 == 0);
  U64 inv_mat4_count = inv_mats.used / 16;
  M_AssertAlways(inv_mat_names.used == inv_mat4_count);

  Pr_Add(out2, S8Lit("// Mat4 count: "));
  Pr_AddU64(out2, inv_mat4_count);
  Pr_Add(out2, S8Lit("\n"));
  Pr_Add(out2, S8Lit("static float4x4 joint_inv_bind_mats[] =\n{"));
  ForU64(mat_index, inv_mat4_count)
  {
    Pr_Add(out2, S8Lit("\n  // ("));
    Pr_AddU64(out2, mat_index);
    Pr_Add(out2, S8Lit(") "));
    Pr_Add(out2, *M_BufferAtS8(&inv_mat_names, mat_index));
    ForU64(number_index, 16)
    {
      U64 i = mat_index*16 + number_index;
      if (number_index % 4 == 0)
        Pr_Add(out2, S8Lit("\n  "));
      Pr_AddFloat(out2, *M_BufferAtFloat(&inv_mats, i));
      Pr_Add(out2, S8Lit("f,"));
    }
  }
  Pr_Add(out2, S8Lit("\n};\n\n"));


  // Reset tmp arena
  Arena_PopScope(tmp_scope);
}
