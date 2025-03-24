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

static void M_GLTF_ExportSkeleton(Printer *p, cgltf_data *data)
{
  M_Check(data->skins_count == 1);
  cgltf_skin *skin = data->skins;

  Pr_S8(p, S8Lit("static AN_Skeleton Worker_Skeleton = {\n"));

  // anims[] + anims_count
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

        U64 target_joint_index = M_FindJointIndex(skin, channel->target_node);
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

          float *numbers = Alloc(M.tmp, float, number_count);
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

          float *numbers = Alloc(M.tmp, float, number_count);
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

      float *numbers = Alloc(M.tmp, float, number_count);
      U64 unpacked = cgltf_accessor_unpack_floats(inv_bind, numbers, number_count);
      M_Check(unpacked == number_count);
      Pr_FloatArray(p, numbers, number_count);

      Pr_S8(p, S8Lit("\n},\n"));
    }

    // child hierarchy indices
    {
      U32 *indices = AllocZeroed(M.tmp, U32, skin->joints_count);
      U32 indices_count = 0;
      RngU32 *ranges = AllocZeroed(M.tmp, RngU32, skin->joints_count);

      ForU64(joint_index, skin->joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];

        ForU64(child_index, joint->children_count)
        {
          cgltf_node *child = joint->children[child_index];
          U64 child_joint_index = M_FindJointIndex(skin, child);

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

  Pr_S8(p, S8Lit("};\n"));
}

static void M_GLTF_Load(const char *path, Printer *out, Printer *out_a)
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
  M_Buffer colors        = M_BufferAlloc(M.tmp, max_verts*1, sizeof(U32)); // this doesn't need such a big array actually

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
        U64 index_start_offset = positions.used/3;

        cgltf_accessor *accessor = primitive->indices;
        M_Check(accessor->component_type == cgltf_component_type_r_16u);
        M_Check(accessor->type == cgltf_type_scalar);

        U16 *numbers = M_BufferPushU16(&indices, accessor->count);
        U64 unpacked = cgltf_accessor_unpack_indices(accessor, numbers, indices.elem_size, accessor->count);
        M_Check(unpacked == accessor->count);

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
            M_Check(comp_count == 3);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_normal:
          {
            save_buf = &normals;
            M_Check(comp_count == 3);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_texcoord:
          {
            save_buf = &texcoords;
            M_Check(comp_count == 2);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_joints:
          {
            save_buf = &joint_indices;
            M_Check(comp_count == 4);
            M_Check(accessor->component_type == cgltf_component_type_r_8u);
          } break;

          case cgltf_attribute_type_weights:
          {
            save_buf = &weights;
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

        U64 total_count = comp_count * accessor->count;
        U64 unpacked = 0;
        if (save_buf->elem_size == 4)
        {
          float *numbers = M_BufferPushFloat(save_buf, total_count);
          unpacked = cgltf_accessor_unpack_floats(accessor, numbers, total_count);
          M_Check(unpacked == total_count);
        }
        else if (save_buf->elem_size == 2)
        {
          U16 *numbers = M_BufferPushU16(save_buf, total_count);
          unpacked = cgltf_accessor_unpack_indices(accessor, numbers, save_buf->elem_size, total_count);
          M_Check(unpacked == total_count);
        }
        else if (save_buf->elem_size == 1)
        {
          U8 *numbers = M_BufferPushU8(save_buf, total_count);
          unpacked = cgltf_accessor_unpack_indices(accessor, numbers, save_buf->elem_size, total_count);
        }
        M_Check(unpacked == total_count);

        // @todo this is a temporary hack that saves a color per position
        if (attribute->type == cgltf_attribute_type_position)
        {
          ForU64(vert_i, accessor->count)
          {
            cgltf_material* material = primitive->material;
            V4 material_color = *(V4 *)material->pbr_metallic_roughness.base_color_factor;
            static_assert(sizeof(material->pbr_metallic_roughness.base_color_factor) == sizeof(material_color));

            U32 *color_value = M_BufferPushU32(&colors, 1);
            *color_value = Color32_V4(material_color);
          }
        }
      }
    }
  }

  M_GLTF_ExportSkeleton(out_a, data);

  //
  // Output collected data
  //
  U64 vert_count = positions.used / 3;
  M_Check(positions.used % 3 == 0);
  M_Check(normals.used % 3 == 0);
  M_Check(vert_count == normals.used / 3);
  M_Check(joint_indices.used % 4 == 0);
  M_Check(vert_count == joint_indices.used / 4);
  M_Check(weights.used % 4 == 0);
  M_Check(vert_count == weights.used / 4);

  M_Check(data->skins_count == 1);
  U64 joints_count = data->skins[0].joints_count;

  Pr_S8(out, S8Lit("static RDR_SkinnedVertex Model_Worker_vrt[] =\n{\n"));
  ForU64(vert_i, vert_count)
  {
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
    Pr_Float(out, *M_BufferAtFloat(&positions, vert_i*3 + 0));
    Pr_S8(out, S8Lit("f,"));
    Pr_Float(out, *M_BufferAtFloat(&positions, vert_i*3 + 1));
    Pr_S8(out, S8Lit("f,"));
    Pr_Float(out, *M_BufferAtFloat(&positions, vert_i*3 + 2));
    Pr_S8(out, S8Lit("f, "));

    Pr_S8(out, S8Lit("/*col*/0x"));
    Pr_U32Hex(out, *M_BufferAtU32(&colors, vert_i));
    Pr_S8(out, S8Lit(", "));

    U32 joint_index_vals[4] =
    {
      *M_BufferAtU8(&joint_indices, vert_i*4 + 0),
      *M_BufferAtU8(&joint_indices, vert_i*4 + 1),
      *M_BufferAtU8(&joint_indices, vert_i*4 + 2),
      *M_BufferAtU8(&joint_indices, vert_i*4 + 3),
    };
    ForArray(i, joint_index_vals)
    {
      if (joint_index_vals[i] >= joints_count)
        M_LOG(M_LogGltfWarning, "[GLTF LOADER] Joint index overflow. %u >= %llu",
              joint_index_vals[i], joints_count);
    }

    U32 joint_packed4 = (joint_index_vals[0] | (joint_index_vals[1] << 8) |
                         (joint_index_vals[2] << 16) | (joint_index_vals[3] << 24));

    Pr_S8(out, S8Lit("/*jnt*/0x"));
    Pr_U32Hex(out, joint_packed4);
    Pr_S8(out, S8Lit(", "));

    float w0 = *M_BufferAtFloat(&weights, vert_i*4 + 0);
    float w1 = *M_BufferAtFloat(&weights, vert_i*4 + 1);
    float w2 = *M_BufferAtFloat(&weights, vert_i*4 + 2);
    float w3 = *M_BufferAtFloat(&weights, vert_i*4 + 3);
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

    Pr_S8(out, S8Lit("\n"));
  }
  Pr_S8(out, S8Lit("};\n\n"));


  Pr_S8(out, S8Lit("static U16 Model_Worker_ind[] =\n{"));
  ForU64(i, indices.used)
  {
    U64 per_group = 3;
    U64 per_row = per_group * 6;
    if (i % per_row == 0)
      Pr_S8(out, S8Lit("\n  "));
    else if ((i % per_row) % per_group == 0)
      Pr_S8(out, S8Lit(" "));

    Pr_U16(out, *M_BufferAtU16(&indices, i));
    Pr_S8(out, S8Lit(","));
  }
  Pr_S8(out, S8Lit("\n};\n\n"));


  // Reset tmp arena
  Arena_PopScope(tmp_scope);
}
