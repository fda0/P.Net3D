#import "cgltf";

BK_GLTF_Spec :: struct
{
  height: float; // automatically adjust scale so model is equal height (meters) on Z axis
  disable_z0: bool; // automatically move model to start at z = 0

  scale := 1.0;
  rot: Quat;
  move: V3;

  name: string; // If empty name will be taken from file name.
};

BK_GLTF_Mesh :: struct
{
  // BK_GLTF_Mesh Groups all vertices and indicecs per common material in .gltf file.
  material_index: s64;
  verts_count: u32;
  indices:       [..] u16;
  positions:     [..] float;
  normals:       [..] float;
  texcoords:     [..] float;
  joint_indices: [..] u8;
  weights:       [..] float;
};

BK_GLTF_Model :: struct
{
  meshes: [..] BK_GLTF_Mesh;

  name: string;
  has_skeleton: bool;
  skeleton_index: u32;
  is_skinned: bool;
  joints_count: u32;
  spec: BK_GLTF_Spec;
  spec_transform: Mat4;
};

BK_GLTF_Load :: (file_path: string, spec: BK_GLTF_Spec)
{
  options := cgltf_options.{
    // memory = .{
    //   alloc_func = M_FakeMalloc,
    //   free_func = M_FakeFree,
    // },
  };

  // Parse .gltf file & load buffers
  data: *cgltf_data;
  res := cgltf_parse_file(*options, temp_c_string(file_path), *data);
  if res != .cgltf_result_success
  {
    log_error("[GLTF LOADER] Failed to parse %", file_path);
    return;
  }

  res = cgltf_load_buffers(*options, data, temp_c_string(file_path));
  if res != .cgltf_result_success
  {
    log_error("[GLTF LOADER] Failed to load buffers, %", file_path);
    return;
  }

  res = cgltf_validate(data);
  if res != .cgltf_result_success
  {
    log_error("[GLTF LOADER] Failed to validate data, %", file_path);
    return;
  }

  // Prepare strings based on file_path
  dir_path, file_name_no_ext, _, _ := path_decomp(file_path);

  // Collect data from .gltf file
  // @todo Arena_Scope scratch = Arena_PushScope(BAKER.tmp);
  max_indices := 1024*256;
  max_verts := 1024*128;

  model: BK_GLTF_Model;
  model.name = ifx spec.name then spec.name else file_name_no_ext;
  model.spec = spec;
  model.is_skinned = data.animations_count > 0;
  array_resize(*model.meshes, xx data.materials_count);

  for * bk_mesh, mesh_index: model.meshes
  {
    // Fetch material data
    assert(mesh_index < xx data.materials_count);
    gltf_material := data.materials[mesh_index];

    // Create material
    bk_mesh.material_index = B.pie.materials.count;
    {
      pie_material := array_add(*B.pie.materials);
      BK_SetDefaultsPIEMaterial(pie_material);

      // Fill material name
      {
        PIE_ListStart(*B.pie.file, *pie_material.name, u8);
        print_to_builder(*B.pie.file, "%.m%.%", model.name, mesh_index, gltf_material.name);
        PIE_ListEnd(*B.pie.file, *pie_material.name);
      }

      diffuse_tex_path := "";
      normal_tex_path := "";
      specular_tex_path := "";

      if gltf_material.normal_texture.texture
      {
        uri := to_string(gltf_material.normal_texture.texture.image.uri);
        normal_tex_path = tprint("%/%", dir_path, uri);
      }

      if gltf_material.has_pbr_metallic_roughness
      {
        metal := *gltf_material.pbr_metallic_roughness;
        pie_material.params.diffuse   = xx Color32_RGBAf(metal.base_color_factor);
        pie_material.params.roughness = metal.roughness_factor;
      }

      if gltf_material.has_pbr_specular_glossiness
      {
        gloss := *gltf_material.pbr_specular_glossiness;
        pie_material.params.diffuse   = xx Color32_RGBAf(gloss.diffuse_factor);
        pie_material.params.specular  = xx Color32_RGBf(gloss.specular_factor);
        pie_material.params.roughness = 1.0 - gloss.glossiness_factor;

        if gloss.diffuse_texture.texture
        {
          uri := to_string(gloss.diffuse_texture.texture.image.uri);
          diffuse_tex_path = tprint("%/%", dir_path, uri);
        }

        if gloss.specular_glossiness_texture.texture
        {
          uri := to_string(gloss.specular_glossiness_texture.texture.image.uri);
          specular_tex_path = tprint(dir_path, uri);
        }
      }

      if (gltf_material.alpha_mode != .cgltf_alpha_mode_opaque)
        pie_material.params.flags |= .HasAlpha;

      // Prepare tex_paths array
      // These are expected to be in a fixed order for now
      // So if any in the chain is missing we have to drop all subsequent textures.
      tex_paths: [3] string;
      tex_paths_count := 0;

      if diffuse_tex_path
      {
        tex_paths[tex_paths_count] = diffuse_tex_path;
        tex_paths_count += 1;
      }

      if tex_paths_count == 1 && normal_tex_path
      {
        tex_paths[tex_paths_count] = normal_tex_path;
        tex_paths_count += 1;
      }

      if tex_paths_count == 2 && specular_tex_path
      {
        tex_paths[tex_paths_count] = specular_tex_path;
        tex_paths_count += 1;
      }

      if tex_paths_count
        BK_TEX_LoadPaths(pie_material, ArrayPrefix(tex_paths, tex_paths_count));
    }
  }

  // The hierarchy of .gltf files might be a bit confusing.
  // Division into meshes and primitives wasn't strictly
  // necessary (it could have been merged into one) but it is what it is.
  for gltf_mesh_index: MakeRange(data.meshes_count)
  {
    gltf_mesh := *data.meshes[gltf_mesh_index];

    for gltf_primitive_index: MakeRange(gltf_mesh.primitives_count)
    {
      primitive := *gltf_mesh.primitives[gltf_primitive_index];

      // Our internal groups things into common meshes based on common material.
      mesh_index := BK_GLTF_FindMaterialIndex(data, primitive.material);
      bk_mesh := *model.meshes[mesh_index];

      if primitive.type != .cgltf_primitive_type_triangles
      {
        log("[GLTF LOADER] Primitive with non triangle type (%) skipped", primitive.type, flags=.WARNING);
        continue;
      }

      // Unpack indices from gltf primitive to bk_mesh.
      Check(primitive.indices != null);
      {
        index_start_offset: u64 = xx (bk_mesh.positions.count / 3);

        accessor := primitive.indices;
        Check(accessor.type == .cgltf_type_scalar);
        numbers := BK_GLTF_UnpackAccessor(accessor, *bk_mesh.indices);

        // Since we are possibly merging multiple primitives into one mesh
        // (we batch meshes based on common material) - we have to add
        // index offset to the indicies from gltf primitve.
        if index_start_offset
        {
          for MakeRange(accessor.count)
          {
            value := numbers[it] + index_start_offset;
            Check(value <= U16_MAX);
            numbers[it] = cast,no_check(u16) value;
          }
        }
      }

      for attribute_index: MakeRange(primitive.attributes_count)
      {
        attribute := *primitive.attributes[attribute_index];
        accessor := attribute.data;

        comp_count := cgltf_num_components(accessor.type);

        if attribute.type == {
          case .cgltf_attribute_type_position;
          Check(comp_count == 3);
          Check(accessor.component_type == .cgltf_component_type_r_32f);
          BK_GLTF_UnpackAccessor(accessor, *bk_mesh.positions);

          case .cgltf_attribute_type_normal;
          Check(comp_count == 3);
          Check(accessor.component_type == .cgltf_component_type_r_32f);
          BK_GLTF_UnpackAccessor(accessor, *bk_mesh.normals);

          case .cgltf_attribute_type_tangent;
          log("[GLTF LOADER] Attribute with type tangent skipped", flags=.VERBOSE_ONLY);
          continue;

          case .cgltf_attribute_type_texcoord;
          Check(comp_count == 2);
          Check(accessor.component_type == .cgltf_component_type_r_32f);
          BK_GLTF_UnpackAccessor(accessor, *bk_mesh.texcoords);

          case .cgltf_attribute_type_joints;
          Check(comp_count == 4);
          //Check(accessor.component_type == .cgltf_component_type_r_8u);
          BK_GLTF_UnpackAccessor(accessor, *bk_mesh.joint_indices);

          case .cgltf_attribute_type_weights;
          Check(comp_count == 4);
          Check(accessor.component_type == .cgltf_component_type_r_32f);
          BK_GLTF_UnpackAccessor(accessor, *bk_mesh.weights);

          case;
          log("[GLTF LOADER] Attribute with unknown type (%) skipped", attribute.type, flags=.WARNING);
          continue;
        }
      }
    }
  }

  // Validate buffer counts
  for * model.meshes
  {
    Check(it.positions.count % 3 == 0);
    it.verts_count = xx (it.positions.count / 3);

    Check(it.normals.count % 3 == 0);
    Check(it.verts_count == it.normals.count / 3);

    if model.is_skinned
    {
      Check(it.joint_indices.count % 4 == 0);
      Check(it.verts_count == it.joint_indices.count / 4);
      Check(it.weights.count % 4 == 0);
      Check(it.verts_count == it.weights.count / 4);
    }
  }

  // Calculate model spec transforms
  {
    // query highest and lowest Z position
    biggest_z := -FLOAT32_MAX;
    lowest_z := FLOAT32_MAX;
    for bk_mesh: model.meshes
    {
      for MakeRange(bk_mesh.verts_count)
      {
        p := V3.{
          bk_mesh.positions[it*3 + 0],
          bk_mesh.positions[it*3 + 1],
          bk_mesh.positions[it*3 + 2],
        };
        p = Rotate(p, spec.rot); // We want to query Z after rotation is applied.
        biggest_z = max(biggest_z, p.z);
        lowest_z = min(lowest_z, p.z);
      }
    }

    height_fix := 1.0;
    if spec.height
    {
      current_height := biggest_z - lowest_z;
      height_fix = spec.height / current_height;
    }

    ground_fix: V3;
    if !spec.disable_z0
    {
      ground_fix = V3.{0,0,-lowest_z};
    }

    // Apply scales to ground fix
    total_scale := spec.scale * height_fix;
    ground_fix = ground_fix * total_scale;
    total_move := spec.move + ground_fix;

    // Get spec transforms
    mat_scale := ScaleMatrix(total_scale);
    mat_rotate := RotationMatrix(spec.rot);
    mat_move := TranslationMatrix(total_move);

    // Calculate final transform
    model.spec_transform = (mat_move * (mat_rotate * mat_scale));
  }

  if model.is_skinned
  {
    // Joints
    Check(data.skins_count == 1);
    Check(data.skins[0].joints_count <= U32_MAX);
    model.joints_count = data.skins[0].joints_count.(u32);
  }

  // Sekeleton export
  if model.is_skinned
  {
    BK_GLTF_ExportSkeletonToPie(*model, data);
  }

  if !model.is_skinned // apply transformations directly to rigid mesh
  {
    if model.spec_transform != DiagonalMatrix()
    {
      for bk_mesh: model.meshes
      {
        for MakeRange(bk_mesh.verts_count)
        {
          position := (*bk_mesh.positions[it*3]).(*V3);
          p :=  model.spec_transform * V4.{xyz = position, w = 1.0};
          position.* = p.xyz;

          normal := (*bk_mesh.normals[it*3]).(*V3);
          n := Rotate(normal.*, spec.rot);
          normal.* = n;
        }
      }
    }
  }

  // Vertices export
  BK_GLTF_ExportModelToPie(model);
}

BK_GLTF_ExportSkeletonToPie :: (model: *BK_GLTF_Model, data: *cgltf_data)
{
  model.has_skeleton = true;
  model.skeleton_index = xx B.pie.skeletons.count;
  pie_skel := array_add(*B.pie.skeletons);

  // Root transform
  pie_skel.root_transform = model.spec_transform;

  Check(data.skins_count == 1);
  skin := data.skins;
  joints_count := skin.joints_count;

  // Joints data
  {
    // unpack inverse bind matrices
    {
      inv_bind := skin.inverse_bind_matrices;
      Check(inv_bind != null);
      Check(inv_bind.count == joints_count);
      Check(inv_bind.component_type == .cgltf_component_type_r_32f);
      Check(inv_bind.type == .cgltf_type_mat4);

      comp_count := cgltf_num_components(inv_bind.type);
      Check(comp_count == 16);

      auto_release_temp();
      temp_matrices := NewArray(xx inv_bind.count, Mat4,, temp);
      float_count := comp_count * inv_bind.count;
      unpacked_count := cgltf_accessor_unpack_floats(inv_bind, temp_matrices.data.(*float), float_count);
      Check(unpacked_count == float_count);
      PIE_ListAddArray(*B.pie.file, *pie_skel.inverse_matrices, temp_matrices);
    }

    // child hierarchy indices
    // {
    //   U32 *indices = PIE_ListReserve(&B.pie.file, &pie_skel.child_index_buf, U32, joints_count);
    //   RngU32 *ranges = PIE_ListReserve(&B.pie.file, &pie_skel.child_index_ranges, RngU32, joints_count);

    //   U32 indices_count = 0;
    //   ForU64(joint_index, joints_count)
    //   {
    //     cgltf_node *joint = skin.joints[joint_index];

    //     ForU64(child_index, joint.children_count)
    //     {
    //       cgltf_node *child = joint.children[child_index];
    //       U64 child_joint_index = BK_GLTF_FindJointIndex(skin, child);

    //       if (child_index == 0)
    //         ranges[joint_index].min = indices_count;

    //       M_Check(indices_count < joints_count);
    //       indices[indices_count] = child_joint_index;
    //       indices_count += 1;

    //       ranges[joint_index].max = indices_count;
    //     }
    //   }
    // }

    // rest pose transformations
    // {
    //   // Translations
    //   PIE_ListStart(&B.pie.file, &pie_skel.translations, TYPE_V3);
    //   ForU64(joint_index, joints_count)
    //   {
    //     cgltf_node *joint = skin.joints[joint_index];
    //     ForArray(i, joint.translation)
    //       *PIE_Reserve(&B.pie.file, float, 1) = joint.translation[i];
    //   }
    //   PIE_ListEnd(&B.pie.file, &pie_skel.translations);

    //   // Rotations
    //   PIE_ListStart(&B.pie.file, &pie_skel.rotations, TYPE_Quat);
    //   ForU64(joint_index, joints_count)
    //   {
    //     cgltf_node *joint = skin.joints[joint_index];
    //     ForArray(i, joint.rotation)
    //       *PIE_Reserve(&B.pie.file, float, 1) = joint.rotation[i];
    //   }
    //   PIE_ListEnd(&B.pie.file, &pie_skel.rotations);

    //   // Scales
    //   PIE_ListStart(&B.pie.file, &pie_skel.scales, TYPE_V3);
    //   ForU64(joint_index, joints_count)
    //   {
    //     cgltf_node *joint = skin.joints[joint_index];
    //     ForArray(i, joint.scale)
    //       *PIE_Reserve(&B.pie.file, float, 1) = joint.scale[i];
    //   }
    //   PIE_ListEnd(&B.pie.file, &pie_skel.scales);
    // }

    // First allocate all strings in one continuous chunk.
    // Track min&max paris of each string.
  //   RngU32 *name_ranges = AllocZeroed(BAKER.tmp, RngU32, joints_count);
  //   ForU64(joint_index, joints_count)
  //   {
  //     cgltf_node *joint = skin.joints[joint_index];
  //     name_ranges[joint_index].min = B.pie.file.used;
  //     Pr_Cstr(&B.pie.file, joint.name);
  //     name_ranges[joint_index].max = B.pie.file.used;
  //   }

  //   // Memcpy name strign ranges into the file
  //   {
  //     RngU32 *dst_name_ranges = PIE_ListReserve(&B.pie.file, &pie_skel.name_ranges, RngU32, joints_count);
  //     Memcpy(dst_name_ranges, name_ranges, pie_skel.name_ranges.size);
  //   }
  }

  // // Animations
  // if (data.animations_count)
  // {
  //   U32 anims_count = (U32)data.animations_count;
  //   PIE_Animation *pie_animations = PIE_ListReserve(&B.pie.file, &pie_skel.anims, PIE_Animation, anims_count);

  //   ForU32(animation_index, anims_count)
  //   {
  //     PIE_Animation *pie_anim = pie_animations + animation_index;
  //     cgltf_animation *gltf_anim = data.animations + animation_index;
  //     float t_min = FLT_MAX;
  //     float t_max = -FLT_MAX;

  //     // Name string
  //     PIE_ListStart(&B.pie.file, &pie_anim.name, TYPE_U8);
  //     Pr_Cstr(&B.pie.file, gltf_anim.name);
  //     PIE_ListEnd(&B.pie.file, &pie_anim.name);

  //     // Channels
  //     U32 channels_count = (U32)gltf_anim.channels_count;
  //     PIE_AnimationChannel *pie_channels = PIE_ListReserve(&B.pie.file, &pie_anim.channels,
  //                                                             PIE_AnimationChannel, channels_count);

  //     ForU32(channel_index, channels_count)
  //     {
  //       PIE_AnimationChannel *pie_chan = pie_channels + channel_index;
  //       cgltf_animation_channel *gltf_chan = gltf_anim.channels + channel_index;
  //       cgltf_animation_sampler *gltf_sampler = gltf_chan.sampler;
  //       M_Check(gltf_sampler.interpolation == cgltf_interpolation_type_linear);
  //       M_Check(gltf_sampler.input.count == gltf_sampler.output.count);

  //       U32 sample_count = (U32)gltf_sampler.input.count;
  //       pie_chan.joint_index = BK_GLTF_FindJointIndex(skin, gltf_chan.target_node);

  //       switch (gltf_chan.target_path)
  //       {
  //         default:
  //         M_Check(false && "Unsupported channel target"); break;

  //         case cgltf_animation_path_type_translation:
  //         pie_chan.type = AN_Translation; break;

  //         case cgltf_animation_path_type_rotation:
  //         pie_chan.type = AN_Rotation; break;

  //         case cgltf_animation_path_type_scale:
  //         pie_chan.type = AN_Scale; break;
  //       }

  //       // inputs
  //       float *in_nums = PIE_ListReserve(&B.pie.file, &pie_chan.inputs, float, sample_count);
  //       U64 in_unpacked = cgltf_accessor_unpack_floats(gltf_sampler.input, in_nums, sample_count);
  //       M_Check(in_unpacked == sample_count);

  //       ForU32(i, sample_count)
  //       {
  //         float n = in_nums[i];
  //         if (n > t_max) t_max = n;
  //         if (n < t_min) t_min = n;
  //       }

  //       // outputs
  //       U32 out_comp_count = (U32)cgltf_num_components(gltf_sampler.output.type);
  //       U32 out_count = out_comp_count * sample_count;
  //       float *out_nums = PIE_ListReserve(&B.pie.file, &pie_chan.outputs, float, out_count);
  //       U64 out_unpacked = cgltf_accessor_unpack_floats(gltf_sampler.output, out_nums, out_count);
  //       M_Check(out_unpacked == out_count);
  //     }

  //     pie_anim.t_min = t_min;
  //     pie_anim.t_max = t_max;
  //   }
  // }
}

BK_GLTF_ExportModelToPie :: (bk_model: BK_GLTF_Model)
{
  // @impl
}

BK_GLTF_FindJointIndex :: (skin: *cgltf_skin, find_node: *cgltf_node) -> u64
{
  for joint_index: MakeRange(skin.joints_count)
    if find_node == skin.joints[joint_index]
      return joint_index;

  Check(false, "Joint index not found");
  return skin.joints_count;
}

BK_GLTF_FindMaterialIndex :: (data: *cgltf_data, find_material: *cgltf_material) -> u32
{
  for material_index: MakeRange(data.materials_count.(u32))
    if find_material == data.materials + material_index
      return material_index;

  Check(false, "Material index not found");
  return data.materials_count.(u32);
}

BK_GLTF_UnpackAccessor :: (accessor: *cgltf_accessor, buffer: *[..] $T) -> slice: [] T
{
  comp_count := cgltf_num_components(accessor.type);
  total_count := accessor.count * comp_count;
  comp_size := cgltf_component_size(accessor.component_type);

  #if T == float
  {
    {
      Check(accessor.component_type == .cgltf_component_type_r_32f,
            "Expected component_type to be r_32f when buffer type is float. Instead got %.",
            accessor.component_type);

      numbers_slice := array_add_many(buffer, xx total_count);
      unpacked_count := cgltf_accessor_unpack_floats(accessor, numbers_slice.data, total_count);
      Check(unpacked_count == total_count);
      return numbers_slice;
    }
  }

  numbers_slice := array_add_many(buffer, xx total_count);

  if size_of(T) >= comp_size
  {
    unpacked_count := cgltf_accessor_unpack_indices(accessor, numbers_slice.data, size_of(T), total_count);
    Check(unpacked_count == total_count);
  }
  else
  {
    // Alloc tmp buffer
    auto_release_temp();
    bytes_to_allocate := comp_size * total_count;
    tmp_nums := talloc(xx bytes_to_allocate);

    // Unpack big integers into tmp_nums
    unpacked_count := cgltf_accessor_unpack_indices(accessor, tmp_nums, comp_size, total_count);
    Check(unpacked_count == total_count);

    // Copy into numbers_slice and check against truncation
    for MakeRange(total_count)
    {
      // Load value from tmp_nums into u64
      value: u64;
      if comp_size == {
        case 8; value = tmp_nums.(*u64)[it];
        case 4; value = tmp_nums.(*u32)[it];
        case 2; value = tmp_nums.(*u16)[it];
        case 1; value = tmp_nums.(*u8)[it];
        case; Check(false, "Unsupported comp_size downconversion");
      }

      // Store into buffer backed array with truncation
      #if T == {
        case u8;
        Check(value <= U8_MAX);
        numbers_slice[it] = cast,no_check(u8) value;
        case u16;
        Check(value <= U16_MAX);
        numbers_slice[it] = cast,no_check(u16) value;
        case u32;
        Check(value <= U32_MAX);
        numbers_slice[it] = cast,no_check(u32) value;
        case;
        Check(false, "Downconversion for % is not supported.", T);
      }
    }
  }

  return numbers_slice;
}
