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
  ForU64(joint_index, skin->joints_count)
    if (find_node == skin->joints[joint_index])
      return joint_index;

  M_Check(false && "Joint index not found");
  return skin->joints_count;
}

static U32 BK_GLTF_FindMaterialIndex(cgltf_data *data, cgltf_material *find_material)
{
  ForU32(material_index, (U32)data->materials_count)
    if (find_material == data->materials + material_index)
    return material_index;

  M_Check(false && "Material index not found");
  return (U32)data->materials_count;
}

static void BK_GLTF_ExportSkeletonToPie(PIE_Builder *build, BK_GLTF_Model *model, cgltf_data *data)
{
  if (model->has_skeleton)
    return;

  model->has_skeleton = true;
  model->skeleton_index = build->skeletons_count;
  build->skeletons_count += 1;

  PIE_Skeleton *pie_skel = PIE_Reserve(&build->skeletons, PIE_Skeleton, 1);

  // Root transform
  pie_skel->root_transform = model->spec_transform;

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
      M_Check(comp_count == 16);

      Mat4 *matrices = PIE_ListReserve(&build->file, &pie_skel->inv_bind_mats, Mat4, (U32)inv_bind->count);

      U64 float_count = comp_count * inv_bind->count;
      U64 unpacked = cgltf_accessor_unpack_floats(inv_bind, matrices->flat, float_count);
      M_Check(unpacked == float_count);
    }

    // child hierarchy indices
    {
      U32 *indices = PIE_ListReserve(&build->file, &pie_skel->child_index_buf, U32, joints_count);
      RngU32 *ranges = PIE_ListReserve(&build->file, &pie_skel->child_index_ranges, RngU32, joints_count);

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
      PIE_ListStart(&build->file, &pie_skel->translations, TYPE_V3);
      ForU64(joint_index, joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];
        ForArray(i, joint->translation)
          *PIE_Reserve(&build->file, float, 1) = joint->translation[i];
      }
      PIE_ListEnd(&build->file, &pie_skel->translations);

      // Rotations
      PIE_ListStart(&build->file, &pie_skel->rotations, TYPE_Quat);
      ForU64(joint_index, joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];
        ForArray(i, joint->rotation)
          *PIE_Reserve(&build->file, float, 1) = joint->rotation[i];
      }
      PIE_ListEnd(&build->file, &pie_skel->rotations);

      // Scales
      PIE_ListStart(&build->file, &pie_skel->scales, TYPE_V3);
      ForU64(joint_index, joints_count)
      {
        cgltf_node *joint = skin->joints[joint_index];
        ForArray(i, joint->scale)
          *PIE_Reserve(&build->file, float, 1) = joint->scale[i];
      }
      PIE_ListEnd(&build->file, &pie_skel->scales);
    }

    // First allocate all strings in one continuous chunk.
    // Track min&max paris of each string.
    RngU32 *name_ranges = AllocZeroed(BAKER.tmp, RngU32, joints_count);
    ForU64(joint_index, joints_count)
    {
      cgltf_node *joint = skin->joints[joint_index];
      name_ranges[joint_index].min = build->file.used;
      Pr_Cstr(&build->file, joint->name);
      name_ranges[joint_index].max = build->file.used;
    }

    // Memcpy name strign ranges into the file
    {
      RngU32 *dst_name_ranges = PIE_ListReserve(&build->file, &pie_skel->name_ranges, RngU32, joints_count);
      Memcpy(dst_name_ranges, name_ranges, pie_skel->name_ranges.size);
    }
  }

  // Animations
  if (data->animations_count)
  {
    U32 anims_count = (U32)data->animations_count;
    PIE_Animation *pie_animations = PIE_ListReserve(&build->file, &pie_skel->anims, PIE_Animation, anims_count);

    ForU32(animation_index, anims_count)
    {
      PIE_Animation *pie_anim = pie_animations + animation_index;
      cgltf_animation *gltf_anim = data->animations + animation_index;
      float t_min = FLT_MAX;
      float t_max = -FLT_MAX;

      // Name string
      PIE_ListStart(&build->file, &pie_anim->name, TYPE_U8);
      Pr_Cstr(&build->file, gltf_anim->name);
      PIE_ListEnd(&build->file, &pie_anim->name);

      // Channels
      U32 channels_count = (U32)gltf_anim->channels_count;
      PIE_AnimationChannel *pie_channels = PIE_ListReserve(&build->file, &pie_anim->channels,
                                                              PIE_AnimationChannel, channels_count);

      ForU32(channel_index, channels_count)
      {
        PIE_AnimationChannel *pie_chan = pie_channels + channel_index;
        cgltf_animation_channel *gltf_chan = gltf_anim->channels + channel_index;
        cgltf_animation_sampler *gltf_sampler = gltf_chan->sampler;
        M_Check(gltf_sampler->interpolation == cgltf_interpolation_type_linear);
        M_Check(gltf_sampler->input->count == gltf_sampler->output->count);

        U32 sample_count = (U32)gltf_sampler->input->count;
        pie_chan->joint_index = BK_GLTF_FindJointIndex(skin, gltf_chan->target_node);

        switch (gltf_chan->target_path)
        {
          default:
          M_Check(false && "Unsupported channel target"); break;

          case cgltf_animation_path_type_translation:
          pie_chan->type = AN_Translation; break;

          case cgltf_animation_path_type_rotation:
          pie_chan->type = AN_Rotation; break;

          case cgltf_animation_path_type_scale:
          pie_chan->type = AN_Scale; break;
        }

        // inputs
        float *in_nums = PIE_ListReserve(&build->file, &pie_chan->inputs, float, sample_count);
        U64 in_unpacked = cgltf_accessor_unpack_floats(gltf_sampler->input, in_nums, sample_count);
        M_Check(in_unpacked == sample_count);

        ForU32(i, sample_count)
        {
          float n = in_nums[i];
          if (n > t_max) t_max = n;
          if (n < t_min) t_min = n;
        }

        // outputs
        U32 out_comp_count = (U32)cgltf_num_components(gltf_sampler->output->type);
        U32 out_count = out_comp_count * sample_count;
        float *out_nums = PIE_ListReserve(&build->file, &pie_chan->outputs, float, out_count);
        U64 out_unpacked = cgltf_accessor_unpack_floats(gltf_sampler->output, out_nums, out_count);
        M_Check(out_unpacked == out_count);
      }

      pie_anim->t_min = t_min;
      pie_anim->t_max = t_max;
    }
  }
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
    Arena_Scope scope = Arena_PushScope(BAKER.tmp);
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

static void BK_GLTF_ExportModelToPie(PIE_Builder *build, BK_GLTF_Model *bk_model)
{
  PIE_Model *pie_model = PIE_Reserve(&build->models, PIE_Model, 1);

  // Name string
  PIE_ListStart(&build->file, &pie_model->name, TYPE_U8);
  Pr_S8(&build->file, bk_model->name);
  PIE_ListEnd(&build->file, &pie_model->name);

  pie_model->is_skinned = bk_model->is_skinned;
  pie_model->skeleton_index = bk_model->skeleton_index;

  PIE_Mesh *pie_meshes = PIE_ListReserve(&build->file, &pie_model->meshes, PIE_Mesh, bk_model->meshes_count);

  ForU32(mesh_index, bk_model->meshes_count)
  {
    BK_GLTF_Mesh *bk_mesh = bk_model->meshes + mesh_index;
    PIE_Mesh *pie_mesh = pie_meshes + mesh_index;

    pie_mesh->material_index = bk_mesh->material_index;
    pie_mesh->vertices_start_index = build->vertices.used / sizeof(WORLD_Vertex);

    // vertices
    ForU64(vert_i, bk_mesh->verts_count)
    {
      WORLD_Vertex vert = {};

      // POS
      vert.p.x = *BK_BufferAtFloat(&bk_mesh->positions, vert_i*3 + 0);
      vert.p.y = *BK_BufferAtFloat(&bk_mesh->positions, vert_i*3 + 1);
      vert.p.z = *BK_BufferAtFloat(&bk_mesh->positions, vert_i*3 + 2);

      // NORMAL
      vert.normal = (V3)
      {
        *BK_BufferAtFloat(&bk_mesh->normals, vert_i*3 + 0),
        *BK_BufferAtFloat(&bk_mesh->normals, vert_i*3 + 1),
        *BK_BufferAtFloat(&bk_mesh->normals, vert_i*3 + 2),
      };

      // NORMAL validation
      {
        float normal_lensq = V3_LengthSq(vert.normal);
        if (normal_lensq < 0.001f)
        {
          M_LOG(M_GLTFWarning, "[GLTF LOADER] Found normal equal to zero");
          vert.normal = (V3){0,0,1};
        }
        else if (normal_lensq < 0.9f || normal_lensq > 1.1f)
        {
          M_LOG(M_GLTFWarning, "[GLTF LOADER] Normal wasn't normalized");
          vert.normal = V3_Scale(vert.normal, FInvSqrt(normal_lensq));
        }
      }

      // UV
      vert.uv.x = *BK_BufferAtFloat(&bk_mesh->texcoords, vert_i*2 + 0);
      vert.uv.y = *BK_BufferAtFloat(&bk_mesh->texcoords, vert_i*2 + 1);

      // SKINNED specific
      if (pie_model->is_skinned)
      {
        U32 joint_indices[4] =
        {
          *BK_BufferAtU8(&bk_mesh->joint_indices, vert_i*4 + 0),
          *BK_BufferAtU8(&bk_mesh->joint_indices, vert_i*4 + 1),
          *BK_BufferAtU8(&bk_mesh->joint_indices, vert_i*4 + 2),
          *BK_BufferAtU8(&bk_mesh->joint_indices, vert_i*4 + 3),
        };
        ForArray(i, joint_indices)
        {
          if (joint_indices[i] >= bk_model->joints_count)
          {
            M_LOG(M_GLTFWarning,
                  "[GLTF LOADER] Joint index overflow. %u >= %u",
                  joint_indices[i], bk_model->joints_count);
          }
        }
        vert.joints_packed4 = (joint_indices[0] |
                               (joint_indices[1] << 8) |
                               (joint_indices[2] << 16) |
                               (joint_indices[3] << 24));

        V4 weights = {};
        weights.x = *BK_BufferAtFloat(&bk_mesh->weights, vert_i*4 + 0);
        weights.y = *BK_BufferAtFloat(&bk_mesh->weights, vert_i*4 + 1);
        weights.z = *BK_BufferAtFloat(&bk_mesh->weights, vert_i*4 + 2);
        weights.w = *BK_BufferAtFloat(&bk_mesh->weights, vert_i*4 + 3);
        float weight_sum = weights.x + weights.y + weights.z + weights.w;
        if (weight_sum < 0.9f || weight_sum > 1.1f)
          M_LOG(M_GLTFWarning, "[GLTF LOADER] Weight sum == %f (should be 1)", weight_sum);
        vert.joint_weights = weights;
      }

      *PIE_Reserve(&build->vertices, WORLD_Vertex, 1) = vert;
    }

    // indices
    {
      U16 *src_indices = bk_mesh->indices.vals;
      U32 src_indices_count = bk_mesh->indices.used;

      pie_mesh->indices_start_index = build->indices.used / sizeof(U16);
      pie_mesh->indices_count += src_indices_count;

      U16 *dst = PIE_Reserve(&build->indices, U16, src_indices_count);
      Memcpy(dst, src_indices, sizeof(U16)*src_indices_count);
    }
  }
}

static void BK_GLTF_Load(const char *file_path, BK_GLTF_Spec spec)
{
  PIE_Builder *build = &BAKER.pie_builder;

  // normalize spec
  if (!spec.scale) spec.scale = 1.f;
  if (Memeq(&spec.rot, &(Quat){}, sizeof(Quat))) spec.rot = Quat_Identity();

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
  cgltf_result res = cgltf_parse_file(&options, file_path, &data);
  if (res != cgltf_result_success)
  {
    M_LOG(M_Err, "[GLTF LOADER] Failed to parse %s", file_path);
    return;
  }

  res = cgltf_load_buffers(&options, data, file_path);
  if (res != cgltf_result_success)
  {
    M_LOG(M_Err, "[GLTF LOADER] Failed to load buffers, %s", file_path);
    return;
  }

  res = cgltf_validate(data);
  if (res != cgltf_result_success)
  {
    M_LOG(M_Err, "[GLTF LOADER] Failed to validate data, %s", file_path);
    return;
  }

  //
  // Prepare strings based on file_path
  //
  S8 dir_path = S8_FromCstr(file_path);
  S8_ConsumeChopUntil(&dir_path, S8Lit("/"), S8_FindLast | S8_SlashInsensitive);

  S8 file_name_no_ext = S8_FromCstr(file_path);
  S8_ConsumeSkipUntil(&file_name_no_ext, S8Lit("/"), S8_FindLast | S8_SlashInsensitive);
  S8_ConsumeChopUntil(&file_name_no_ext, S8Lit("."), S8_FindLast | S8_SlashInsensitive);

  //
  // Collect data from .gltf file
  //
  Arena_Scope scratch = Arena_PushScope(BAKER.tmp);
  U64 max_indices = 1024*256;
  U64 max_verts = 1024*128;

  BK_GLTF_Model model = {};
  model.spec = spec;
  model.meshes_count = (U32)data->materials_count;
  model.meshes = AllocZeroed(scratch.a, BK_GLTF_Mesh, model.meshes_count);
  model.name = spec.name;
  if (!model.name.size) model.name = file_name_no_ext;
  model.is_skinned = (data->animations_count > 0);

  ForU32(mesh_index, model.meshes_count)
  {
    BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
    // Prepare bk_mesh buffers
    bk_mesh->indices = BK_BufferAlloc(scratch.a, max_indices, sizeof(U16));
    bk_mesh->positions     = BK_BufferAlloc(scratch.a, max_verts*3, sizeof(float));
    bk_mesh->normals       = BK_BufferAlloc(scratch.a, max_verts*3, sizeof(float));
    bk_mesh->texcoords     = BK_BufferAlloc(scratch.a, max_verts*2, sizeof(float));
    bk_mesh->joint_indices = BK_BufferAlloc(scratch.a, max_verts*4, sizeof(U8));
    bk_mesh->weights       = BK_BufferAlloc(scratch.a, max_verts*4, sizeof(float));

    // Fetch material data
    cgltf_material *gltf_material = data->materials + mesh_index;

    // Create material
    bk_mesh->material_index = build->materials_count;
    {
      build->materials_count += 1;
      PIE_Material *pie_material = PIE_Reserve(&build->materials, PIE_Material, 1);
      BK_SetDefaultsPIEMaterial(pie_material);

      // Fill material name
      {
        PIE_ListStart(&build->file, &pie_material->name, TYPE_U8);
        Pr_S8(&build->file, model.name);
        Pr_Cstr(&build->file, ".m");
        Pr_U32(&build->file, mesh_index);
        Pr_Cstr(&build->file, ".");
        Pr_Cstr(&build->file, gltf_material->name);
        PIE_ListEnd(&build->file, &pie_material->name);
      }

      S8 diffuse_tex_path = {};
      S8 normal_tex_path = {};
      S8 specular_tex_path = {};

      if (gltf_material->normal_texture.texture)
      {
        const char *uri = gltf_material->normal_texture.texture->image->uri;
        normal_tex_path = S8_ConcatDirFile(scratch.a, dir_path, S8_FromCstr(uri));
      }

      if (gltf_material->has_pbr_metallic_roughness)
      {
        cgltf_pbr_metallic_roughness *metal = &gltf_material->pbr_metallic_roughness;
        pie_material->params.diffuse   = Color32_V4(*(V4 *)metal->base_color_factor);
        pie_material->params.roughness = metal->roughness_factor;
      }

      if (gltf_material->has_pbr_specular_glossiness)
      {
        cgltf_pbr_specular_glossiness *gloss = &gltf_material->pbr_specular_glossiness;
        pie_material->params.diffuse   = Color32_V4(*(V4 *)gloss->diffuse_factor);
        pie_material->params.specular  = Color32_V3(*(V3 *)gloss->specular_factor);
        pie_material->params.roughness = 1.f - gloss->glossiness_factor;

        if (gloss->diffuse_texture.texture)
        {
          const char *uri = gloss->diffuse_texture.texture->image->uri;
          diffuse_tex_path = S8_ConcatDirFile(scratch.a, dir_path, S8_FromCstr(uri));
        }

        if (gloss->specular_glossiness_texture.texture)
        {
          const char *uri = gloss->specular_glossiness_texture.texture->image->uri;
          specular_tex_path = S8_ConcatDirFile(scratch.a, dir_path, S8_FromCstr(uri));
        }
      }

      if (gltf_material->alpha_mode != cgltf_alpha_mode_opaque)
        pie_material->params.flags |= PIE_MaterialFlag_HasAlpha;

      // Prepare tex_paths array
      // These are expected to be in a fixed order for now
      // So if any in the chain is missing we have to drop all subsequent textures.
      S8 tex_paths[3] = {};
      U32 tex_paths_count = 0;

      if (diffuse_tex_path.size)
        tex_paths[tex_paths_count++] = diffuse_tex_path;

      if (tex_paths_count == 1 && normal_tex_path.size)
        tex_paths[tex_paths_count++] = normal_tex_path;

      if (tex_paths_count == 2 && specular_tex_path.size)
        tex_paths[tex_paths_count++] = specular_tex_path;

      if (tex_paths_count)
        BK_TEX_LoadPaths(pie_material, tex_paths, tex_paths_count);
    }
  }

  // The hierarchy of .gltf files might be a bit confusing.
  // Division into meshes and primitives wasn't strictly
  // necessary (it could have been merged into one) but it is what it is.
  ForU64(gltf_mesh_index, data->meshes_count)
  {
    cgltf_mesh *gltf_mesh = data->meshes + gltf_mesh_index;

    ForU64(gltf_primitive_index, gltf_mesh->primitives_count)
    {
      cgltf_primitive *primitive = gltf_mesh->primitives + gltf_primitive_index;

      // Our internal groups things into common meshes based on common material.
      U32 mesh_index = BK_GLTF_FindMaterialIndex(data, primitive->material);
      BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;

      if (primitive->type != cgltf_primitive_type_triangles)
      {
        M_LOG(M_GLTFWarning,
              "[GLTF LOADER] Primitive with non triangle type (%u) skipped",
              primitive->type);
        continue;
      }

      M_Check(primitive->indices);
      {
        U64 index_start_offset = bk_mesh->positions.used/3;

        cgltf_accessor *accessor = primitive->indices;
        M_Check(accessor->type == cgltf_type_scalar);
        U16 *numbers = BK_GLTF_UnpackAccessor(accessor, &bk_mesh->indices);

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
            save_buf = &bk_mesh->positions;
            M_Check(comp_count == 3);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_normal:
          {
            save_buf = &bk_mesh->normals;
            M_Check(comp_count == 3);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_tangent:
          {
            M_LOG(M_GLTFDebug, "[GLTF LOADER] Attribute with type tangent skipped");
            continue;
          } break;

          case cgltf_attribute_type_texcoord:
          {
            save_buf = &bk_mesh->texcoords;
            M_Check(comp_count == 2);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          case cgltf_attribute_type_joints:
          {
            save_buf = &bk_mesh->joint_indices;
            M_Check(comp_count == 4);
            //M_Check(accessor->component_type == cgltf_component_type_r_8u);
          } break;

          case cgltf_attribute_type_weights:
          {
            save_buf = &bk_mesh->weights;
            M_Check(comp_count == 4);
            M_Check(accessor->component_type == cgltf_component_type_r_32f);
          } break;

          default:
          {
            M_LOG(M_GLTFWarning,
                  "[GLTF LOADER] Attribute with unknown type (%u) skipped",
                  attribute->type);
            continue;
          } break;
        }

        BK_GLTF_UnpackAccessor(accessor, save_buf);
      }
    }
  }

  //
  //
  //

  // Validate buffer counts
  ForU32(mesh_index, model.meshes_count)
  {
    BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
    M_Check(bk_mesh->positions.used % 3 == 0);
    bk_mesh->verts_count = bk_mesh->positions.used / 3;

    M_Check(bk_mesh->normals.used % 3 == 0);
    M_Check(bk_mesh->verts_count == bk_mesh->normals.used / 3);

    if (model.is_skinned)
    {
      M_Check(bk_mesh->joint_indices.used % 4 == 0);
      M_Check(bk_mesh->verts_count == bk_mesh->joint_indices.used / 4);
      M_Check(bk_mesh->weights.used % 4 == 0);
      M_Check(bk_mesh->verts_count == bk_mesh->weights.used / 4);
    }
  }

  // Calculate model spec transforms
  {
    // query highest and lowest Z position
    float biggest_z = -FLT_MAX;
    float lowest_z = FLT_MAX;
    ForU32(mesh_index, model.meshes_count)
    {
      BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
      ForU32(i, bk_mesh->verts_count)
      {
        V3 p = *(V3*)BK_BufferAtFloat(&bk_mesh->positions, i*3);
        p = V3_Rotate(p, spec.rot); // We want to query Z after rotation is applied.
        biggest_z = Max(biggest_z, p.z);
        lowest_z = Min(lowest_z, p.z);
      }
    }

    float height_fix = 1.f;
    if (spec.height)
    {
      float current_height = biggest_z - lowest_z;
      height_fix = spec.height / current_height;
    }

    V3 ground_fix = (V3){};
    if (!spec.disable_z0)
    {
      ground_fix = (V3){0,0,-lowest_z};
    }

    // Apply scales to ground fix
    float total_scale = spec.scale * height_fix;
    ground_fix = V3_Scale(ground_fix, total_scale);
    V3 total_move = V3_Add(spec.move, ground_fix);

    // Get spec transforms
    Mat4 mat_scale = Mat4_ScaleF(total_scale);
    Mat4 mat_rotate = Mat4_Rotation_Quat(spec.rot);
    Mat4 mat_move = Mat4_Translation(total_move);

    // Calculate final transform
    model.spec_transform = Mat4_Mul(mat_move, Mat4_Mul(mat_rotate, mat_scale));
  }

  if (model.is_skinned)
  {
    // Joints
    M_Check(data->skins_count == 1);
    M_Check(data->skins[0].joints_count <= U32_MAX);
    model.joints_count = (U32)data->skins[0].joints_count;
  }

  // Sekeleton export
  if (model.is_skinned)
  {
    BK_GLTF_ExportSkeletonToPie(build, &model, data);
  }

  if (!model.is_skinned) // apply transformations directly to rigid mesh
  {
    if (!Mat4_IsIdentity(model.spec_transform))
    {
      ForU32(mesh_index, model.meshes_count)
      {
        BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
        ForU32(i, bk_mesh->verts_count)
        {
          V3 *position = (V3*)BK_BufferAtFloat(&bk_mesh->positions, i*3);
          V4 p = V4_From_XYZ_W(*position, 1.f);
          p = V4_MulM4(model.spec_transform, p);

          V3 *normal = (V3*)BK_BufferAtFloat(&bk_mesh->normals, i*3);
          V3 n = V3_Rotate(*normal, spec.rot);

          *position = V3_FromV4_XYZ(p);
          *normal = n;
        }
      }
    }
  }

  // Vertices export
  BK_GLTF_ExportModelToPie(build, &model);

  // Reset tmp arena
  Arena_PopScope(scratch);
}
