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

static void M_GLTF_Load(const char *path, Printer *out, float scale)
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
  U64 max_verts = 1024*128;
  M_Buffer indices   = M_BufferAlloc(M.tmp, max_verts*2, sizeof(U16));
  M_Buffer positions = M_BufferAlloc(M.tmp, max_verts*3, sizeof(float));
  M_Buffer normals   = M_BufferAlloc(M.tmp, max_verts*3, sizeof(float));
  M_Buffer texcoords = M_BufferAlloc(M.tmp, max_verts*2, sizeof(float));
  M_Buffer joints    = M_BufferAlloc(M.tmp, max_verts*4, sizeof(U8));
  M_Buffer weights   = M_BufferAlloc(M.tmp, max_verts*4, sizeof(float));
  M_Buffer inv_mats  = M_BufferAlloc(M.tmp, max_verts*16, sizeof(float));

  ForU64(node_index, data->nodes_count)
  {
    cgltf_node *node = data->nodes + node_index;

    if (node_index != 62)
      continue;

    cgltf_mesh *mesh = node->mesh;
    cgltf_skin *skin = node->skin;
    if (mesh)
    {
      ForU64(primitive_index, mesh->primitives_count)
      {
        cgltf_primitive *primitive = mesh->primitives + primitive_index;

        if (primitive_index != 0)
          continue;

        if (primitive->type != cgltf_primitive_type_triangles)
        {
          M_LOG(M_LogGltfWarning,
                "[GLTF LOADER] Primitive with non triangle type (%u) skipped",
                primitive->type);
          continue;
        }

        M_AssertAlways(primitive->indices);
        {
          cgltf_accessor *accessor = primitive->indices;
          M_AssertAlways(accessor->component_type == cgltf_component_type_r_16u);
          M_AssertAlways(accessor->type == cgltf_type_scalar);

          U16 *numbers = M_BufferPushU16(&indices, accessor->count);
          U64 unpacked = cgltf_accessor_unpack_indices(accessor, numbers, indices.elem_size, accessor->count);
          M_AssertAlways(unpacked == accessor->count);
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
              save_buf = &joints;
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

    if (skin)
    {
      cgltf_accessor *accessor = skin->inverse_bind_matrices;
      M_AssertAlways(accessor);
      M_AssertAlways(accessor->component_type == cgltf_component_type_r_32f);
      M_AssertAlways(accessor->type == cgltf_type_mat4);

      U64 comp_count = cgltf_num_components(accessor->type);
      U64 total_count = comp_count * accessor->count;

      float *numbers = M_BufferPushFloat(&inv_mats, total_count);
      U64 unpacked = cgltf_accessor_unpack_floats(accessor, numbers, total_count);
      M_AssertAlways(unpacked == total_count);
    }
  }

  //
  // Output collected data
  //
  M_AssertAlways(positions.used % 3 == 0);
  U64 positions_count = positions.used / 3;

  M_AssertAlways(normals.used % 3 == 0);
  U64 normals_count = normals.used / 3;

  M_AssertAlways(joints.used % 4 == 0);
  U64 joints_count = joints.used / 4;

  M_AssertAlways(weights.used % 4 == 0);
  U64 weights_count = weights.used / 4;

  M_AssertAlways(positions_count == normals_count);
  M_AssertAlways(positions_count == joints_count);
  M_AssertAlways(positions_count == weights_count);

  Pr_Add(out, S8Lit("static Rdr_SkinnedVertex Model_Worker_vrt[] =\n{\n"));
  ForU64(i, positions_count)
  {
    Pr_Add(out, S8Lit("  /*pos*/"));
    Pr_AddFloat(out, *M_BufferAtFloat(&positions, i*3 + 0) * scale);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, *M_BufferAtFloat(&positions, i*3 + 1) * scale);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, *M_BufferAtFloat(&positions, i*3 + 2) * scale);
    Pr_Add(out, S8Lit("f, "));

    Pr_Add(out, S8Lit("/*col*/1.f,1.f,1.f, "));

    V3 normal =
    {
      *M_BufferAtFloat(&normals, i*3 + 0),
      *M_BufferAtFloat(&normals, i*3 + 1),
      *M_BufferAtFloat(&normals, i*3 + 2),
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

    U32 j0 = *M_BufferAtU8(&joints, i*4 + 0);
    U32 j1 = *M_BufferAtU8(&joints, i*4 + 1);
    U32 j2 = *M_BufferAtU8(&joints, i*4 + 2);
    U32 j3 = *M_BufferAtU8(&joints, i*4 + 3);
    U32 joint_packed4 = (j0 | (j1 << 8) | (j2 << 16) | (j3 << 24));
    Pr_Add(out, S8Lit("/*jnt*/0x"));
    Pr_AddU32Hex(out, joint_packed4);
    Pr_Add(out, S8Lit(", "));

    float w0 = *M_BufferAtFloat(&weights, i*4 + 0);
    float w1 = *M_BufferAtFloat(&weights, i*4 + 1);
    float w2 = *M_BufferAtFloat(&weights, i*4 + 2);
    float w3 = *M_BufferAtFloat(&weights, i*4 + 3);
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

  Pr_Add(out, S8Lit("static float Model_Worker_inv_bind_mat[] =\n{"));
  ForU64(i, inv_mats.used)
  {
    U64 per_row = 4;
    U64 per_mat = per_row * 4;
    if (i % per_mat == 0)
      Pr_Add(out, S8Lit("\n\n  "));
    else if ((i % per_mat) % per_row == 0)
      Pr_Add(out, S8Lit("\n  "));

    Pr_AddFloat(out, *M_BufferAtFloat(&inv_mats, i));
    Pr_Add(out, S8Lit("f, "));
  }
  Pr_Add(out, S8Lit("\n};\n\n"));


  // Reset tmp arena
  Arena_PopScope(tmp_scope);
}
