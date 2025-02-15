static void *M_FakeMalloc(void *user_data, U64 size)
{
  (void)user_data;
  U64 default_align = 16;
  return Arena_AllocateBytes(M.tmp, size, default_align, false);
}
static void M_FakeFree(void *user_data, void *ptr)
{
  (void)user_data;
  (void)ptr;
  // do nothing
}

static void M_GLTF_Load(const char *path)
{
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

          float numbers[4];
          M_AssertAlways(ArrayCount(numbers) >= number_count);

          ForU64(value_index, accessor->count)
          {
            SDL_zeroa(numbers);

            // @todo @speed there is a way to batch-load these floats!
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
}
