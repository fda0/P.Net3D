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

static U64 M_NodeIndex(cgltf_data *data, cgltf_node *target_node)
{
  M_AssertAlways(target_node >= data->nodes);
  M_AssertAlways(target_node < data->nodes + data->nodes_count);
  U64 index = ((U64)target_node - (U64)data->nodes) / sizeof(*target_node);
  return index;
}


static void M_GLTF_Load(const char *path, Printer *out)
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
  M_Buffer inv_mats       = M_BufferAlloc(M.tmp, max_joints*16, sizeof(float));
  M_Buffer inv_mat_names  = M_BufferAlloc(M.tmp, max_joints,    sizeof(S8));
  M_Buffer animation_mats = M_BufferAlloc(M.tmp, max_joints*16, sizeof(float));

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
          {
            numbers[i] += index_start_offset;
          }
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

  ForU64(skin_index, data->skins_count)
  {
    cgltf_skin *skin = data->skins + skin_index;

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

    // tranpose unloaded matrices (row major -> col major)
    {
      Mat4 *mats = (Mat4 *)numbers;
      ForU64(mat_index, total_count/16)
      {
        mats[mat_index] = Mat4_Transpose(mats[mat_index]);
      }
    }


#if 1
    M_AssertAlways(skin->joints_count == (total_count/16));

    // premultiply these matrices ... @todo fix? delete?
    Mat4 *mats = (Mat4 *)numbers;
    ForU64(joint_index, skin->joints_count)
    {
      cgltf_node *joint = skin->joints[joint_index];

      Mat4 joint_mat = Mat4_Identity();
#if 1
      if (joint->has_translation)
      {
        V3 translation = (V3){joint->translation[0], joint->translation[1], joint->translation[2]};
        joint_mat = Mat4_Mul(Mat4_Translation(translation), joint_mat);
      }
#endif
#if 1
      if (joint->has_rotation)
      {
        V3 rotation = (V3){joint->rotation[0], joint->rotation[1], joint->rotation[2]};
        joint_mat = Mat4_Mul(Mat4_Rotation_RH((V3){1,0,0}, rotation.x), joint_mat);
        joint_mat = Mat4_Mul(Mat4_Rotation_RH((V3){0,1,0}, rotation.y), joint_mat);
        joint_mat = Mat4_Mul(Mat4_Rotation_RH((V3){0,0,1}, rotation.z), joint_mat);
      }
#endif
#if 1
      if (joint->has_scale)
      {
        V3 scale = (V3){joint->scale[0], joint->scale[1], joint->scale[2]};
        joint_mat = Mat4_Mul(Mat4_Scale(scale), joint_mat);
      }
#endif

      mats[joint_index] = Mat4_Mul(mats[joint_index], joint_mat);

      // @todo apply joint_mat to joint's children

    }
#endif






    // init anim mats
    {
      ForU64(mat_index, total_count/16)
      {
        Mat4 *anim_mat = (Mat4 *)M_BufferPushFloat(&animation_mats, 16);
        *anim_mat = Mat4_Identity();
      }
    }
  }

  M_AssertAlways(inv_mats.used == animation_mats.used);
  Mat4 *anim_mats = (Mat4 *)animation_mats.vals;

  ForU64(animation_index, data->animations_count)
  {
    cgltf_animation *animation = data->animations + animation_index;

    if (animation_index != 4)
      continue;

    ForU64(channel_index, animation->channels_count)
    {
      cgltf_animation_channel *channel = animation->channels + channel_index;
      cgltf_node *target = channel->target_node; // figure out joint index

      M_AssertAlways(target >= data->nodes);
      M_AssertAlways(target < data->nodes + data->nodes_count);
      U64 target_index = ((U64)target - (U64)data->nodes) / sizeof(*target);
      M_AssertAlways(target_index < inv_mats.used/16);

      Mat4 *target_anim = anim_mats + target_index;

      cgltf_animation_sampler *sampler = channel->sampler;
      M_AssertAlways(sampler->input->count == sampler->output->count);
      U64 mid_index = sampler->input->count / 2;

      float in = 0;
      U64 read_in = cgltf_accessor_read_float(sampler->input, mid_index, &in, 1);
      (void)read_in;
      (void)in;

      float outs[4] = {};
      U64 read_out = 0;
      ForArray(i, outs)
      {
        bool success = cgltf_accessor_read_float(sampler->output, mid_index + i, outs, ArrayCount(outs));
        if (!success)
          break;
        read_out += 1;
      }

      if (channel->target_path == cgltf_animation_path_type_translation)
      {
        M_AssertAlways(read_out >= 3);
        Mat4 translation = Mat4_Translation(*(V3 *)outs);
        *target_anim = Mat4_Mul(*target_anim, translation);
      }
    }
  }

#if 0 // @todo apply rest pose first?
  // modify inv matrices
  ForU64(mat_index, inv_mats.used/16)
  {
    Mat4 *inv  = (Mat4 *)inv_mats.vals + mat_index;
    Mat4 *anim = (Mat4 *)animation_mats.vals + mat_index;
    *inv = Mat4_Mul(*inv, *anim);
  }
#endif

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

  Pr_Add(out, S8Lit("// Mat4 count: "));
  Pr_AddU64(out, inv_mat4_count);
  Pr_Add(out, S8Lit("\n"));
  Pr_Add(out, S8Lit("static float Model_Worker_inv_bind_mat[] =\n{"));
  ForU64(mat_index, inv_mat4_count)
  {
    Pr_Add(out, S8Lit("\n  // "));
    Pr_Add(out, *M_BufferAtS8(&inv_mat_names, mat_index));
    ForU64(number_index, 16)
    {
      U64 i = mat_index*16 + number_index;
      if (number_index % 4 == 0)
        Pr_Add(out, S8Lit("\n  "));
      Pr_AddFloat(out, *M_BufferAtFloat(&inv_mats, i));
      Pr_Add(out, S8Lit("f,"));
    }
  }
  Pr_Add(out, S8Lit("\n};\n\n"));


  // Reset tmp arena
  Arena_PopScope(tmp_scope);
}
