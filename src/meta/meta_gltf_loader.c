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

typedef struct
{
  float *vals;
  U64 used;
  U64 cap;
} M_FloatBuffer;

static M_FloatBuffer M_AllocFloatBuffer(Arena *a, U64 count)
{
  M_FloatBuffer buf = {};
  buf.vals = Alloc(a, float, count);
  buf.cap = count;
  return buf;
}

static float *M_FloatsFromFloatBuffer(M_FloatBuffer *buf, U64 count)
{
  M_AssertAlways(buf->cap >= buf->used);
  M_AssertAlways(count <= (buf->cap - buf->used));
  float *res = buf->vals + buf->used;
  buf->used += count;
  return res;
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
  U64 max_nums = 1024*1024;
  M_FloatBuffer positions = M_AllocFloatBuffer(M.tmp, max_nums);
  M_FloatBuffer normals = M_AllocFloatBuffer(M.tmp, max_nums);

  ForU64(node_index, data->nodes_count)
  {
    cgltf_node *node = data->nodes + node_index;
    cgltf_mesh* mesh = node->mesh;
    if (mesh)
    {
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

        ForU64(attribute_index, primitive->attributes_count)
        {
          cgltf_attribute *attribute = primitive->attributes + attribute_index;
          cgltf_accessor *accessor = attribute->data;

          U32 number_count = 0;
          switch (accessor->type)
          {
            case cgltf_type_scalar: number_count = 1; break;
            case cgltf_type_vec2:   number_count = 2; break;
            case cgltf_type_vec3:   number_count = 3; break;
            case cgltf_type_vec4:   number_count = 4; break;
            default:
            {
              M_LOG(M_LogGltfWarning,
                    "[GLTF LOADER] Accessor with unknown type (%u) skipped",
                    accessor->type);
              continue;
            } break;
          }

          M_FloatBuffer *save_buf = 0;
          switch (attribute->type)
          {
            case cgltf_attribute_type_position:
            {
              save_buf = &positions;
              M_AssertAlways(number_count == 3);
            } break;

            case cgltf_attribute_type_normal:
            {
              save_buf = &normals;
              M_AssertAlways(number_count == 3);
            } break;

            default:
            {
              M_LOG(M_LogGltfWarning,
                    "[GLTF LOADER] Attribute with unknown type (%u) skipped",
                    attribute->type);
              continue;
            } break;
          }

          ForU64(value_index, accessor->count)
          {
            float *numbers = M_FloatsFromFloatBuffer(save_buf, number_count);
            bool ok = cgltf_accessor_read_float(accessor, value_index, numbers, number_count);
            if (ok)
            {
              M_LOG(M_LogGltfDebug,
                    "[GLTF LOADER] Loaded %u numbers",
                    number_count);
            }
            else
            {
              M_LOG(M_LogGltfDebug,
                    "[GLTF LOADER] Failed to load %u numbers",
                    number_count);
            }
          }
        }
      }
    }
  }

  //
  // Output collected data
  //
  M_AssertAlways(positions.used % 3 == 0);
  M_AssertAlways(normals.used % 3 == 0);
  U64 positions_vec_count = positions.used / 3;
  U64 normals_vec_count = normals.used / 3;
  M_AssertAlways(positions_vec_count == normals_vec_count);

  Pr_Add(out, S8Lit("static Rdr_ModelVertex Model_Worker_vrt[] =\n{\n"));
  ForU64(i, positions_vec_count)
  {
    Pr_Add(out, S8Lit("  /*pos*/"));
    Pr_AddFloat(out, positions.vals[i*3 + 0]);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, positions.vals[i*3 + 1]);
    Pr_Add(out, S8Lit("f,"));
    Pr_AddFloat(out, positions.vals[i*3 + 2]);
    Pr_Add(out, S8Lit("f, "));

    Pr_Add(out, S8Lit("/*col*/1.f,1.f,1.f, "));

    V3 normal =
    {
      normals.vals[i*3 + 0],
      normals.vals[i*3 + 1],
      normals.vals[i*3 + 2],
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
    Pr_Add(out, S8Lit("f,\n"));
  }
  Pr_Add(out, S8Lit("};\n\n"));


  // Reset tmp arena
  Arena_PopScope(tmp_scope);
}
