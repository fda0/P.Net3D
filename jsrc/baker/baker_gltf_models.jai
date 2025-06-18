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
      // Check(primitive.indices != null);
      // {
      //   index_start_offset := bk_mesh.positions.count / 3;

      //   accessor := primitive.indices;
      //   Check(accessor.type == cgltf_type_scalar);
      //   numbers := BK_GLTF_UnpackAccessor(accessor, &bk_mesh.indices);

      //   // Since we are possibly merging multiple primitives into one mesh
      //   // (we batch meshes based on common material) - we have to add
      //   // index offset to the indicies from gltf primitve.
      //   if index_start_offset
      //   {
      //     for MakeRange(accessor.count)
      //     {
      //       value := numbers[it] + index_start_offset;
      //       Check(value <= U16_MAX);
      //       numbers[it] = value;
      //     }
      //   }
      // }

  //     ForU64(attribute_index, primitive.attributes_count)
  //     {
  //       cgltf_attribute *attribute = primitive.attributes + attribute_index;
  //       cgltf_accessor *accessor = attribute.data;

  //       U64 comp_count = cgltf_num_components(accessor.type);
  //       BK_Buffer *save_buf = 0;

  //       switch (attribute.type)
  //       {
  //         case cgltf_attribute_type_position:
  //         {
  //           save_buf = &bk_mesh.positions;
  //           M_Check(comp_count == 3);
  //           M_Check(accessor.component_type == cgltf_component_type_r_32f);
  //         } break;

  //         case cgltf_attribute_type_normal:
  //         {
  //           save_buf = &bk_mesh.normals;
  //           M_Check(comp_count == 3);
  //           M_Check(accessor.component_type == cgltf_component_type_r_32f);
  //         } break;

  //         case cgltf_attribute_type_tangent:
  //         {
  //           M_LOG(M_GLTFDebug, "[GLTF LOADER] Attribute with type tangent skipped");
  //           continue;
  //         } break;

  //         case cgltf_attribute_type_texcoord:
  //         {
  //           save_buf = &bk_mesh.texcoords;
  //           M_Check(comp_count == 2);
  //           M_Check(accessor.component_type == cgltf_component_type_r_32f);
  //         } break;

  //         case cgltf_attribute_type_joints:
  //         {
  //           save_buf = &bk_mesh.joint_indices;
  //           M_Check(comp_count == 4);
  //           //M_Check(accessor.component_type == cgltf_component_type_r_8u);
  //         } break;

  //         case cgltf_attribute_type_weights:
  //         {
  //           save_buf = &bk_mesh.weights;
  //           M_Check(comp_count == 4);
  //           M_Check(accessor.component_type == cgltf_component_type_r_32f);
  //         } break;

  //         default:
  //         {
  //           M_LOG(M_GLTFWarning,
  //             "[GLTF LOADER] Attribute with unknown type (%u) skipped",
  //             attribute.type);
  //           continue;
  //         } break;
  //       }

  //       BK_GLTF_UnpackAccessor(accessor, save_buf);
  //     }
    }
  }

  // //
  // //
  // //

  // // Validate buffer counts
  // ForU32(mesh_index, model.meshes_count)
  // {
  //   BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
  //   M_Check(bk_mesh.positions.used % 3 == 0);
  //   bk_mesh.verts_count = bk_mesh.positions.used / 3;

  //   M_Check(bk_mesh.normals.used % 3 == 0);
  //   M_Check(bk_mesh.verts_count == bk_mesh.normals.used / 3);

  //   if (model.is_skinned)
  //   {
  //     M_Check(bk_mesh.joint_indices.used % 4 == 0);
  //     M_Check(bk_mesh.verts_count == bk_mesh.joint_indices.used / 4);
  //     M_Check(bk_mesh.weights.used % 4 == 0);
  //     M_Check(bk_mesh.verts_count == bk_mesh.weights.used / 4);
  //   }
  // }

  // // Calculate model spec transforms
  // {
  //   // query highest and lowest Z position
  //   float biggest_z = -FLT_MAX;
  //   float lowest_z = FLT_MAX;
  //   ForU32(mesh_index, model.meshes_count)
  //   {
  //     BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
  //     ForU32(i, bk_mesh.verts_count)
  //     {
  //       V3 p = *(V3*)BK_BufferAtFloat(&bk_mesh.positions, i*3);
  //       p = V3_Rotate(p, spec.rot); // We want to query Z after rotation is applied.
  //       biggest_z = Max(biggest_z, p.z);
  //       lowest_z = Min(lowest_z, p.z);
  //     }
  //   }

  //   float height_fix = 1.f;
  //   if (spec.height)
  //   {
  //     float current_height = biggest_z - lowest_z;
  //     height_fix = spec.height / current_height;
  //   }

  //   V3 ground_fix = (V3){};
  //   if (!spec.disable_z0)
  //   {
  //     ground_fix = (V3){0,0,-lowest_z};
  //   }

  //   // Apply scales to ground fix
  //   float total_scale = spec.scale * height_fix;
  //   ground_fix = V3_Scale(ground_fix, total_scale);
  //   V3 total_move = V3_Add(spec.move, ground_fix);

  //   // Get spec transforms
  //   Mat4 mat_scale = Mat4_ScaleF(total_scale);
  //   Mat4 mat_rotate = Mat4_Rotation_Quat(spec.rot);
  //   Mat4 mat_move = Mat4_Translation(total_move);

  //   // Calculate final transform
  //   model.spec_transform = Mat4_Mul(mat_move, Mat4_Mul(mat_rotate, mat_scale));
  // }

  // if (model.is_skinned)
  // {
  //   // Joints
  //   M_Check(data.skins_count == 1);
  //   M_Check(data.skins[0].joints_count <= U32_MAX);
  //   model.joints_count = (U32)data.skins[0].joints_count;
  // }

  // // Sekeleton export
  // if (model.is_skinned)
  // {
  //   BK_GLTF_ExportSkeletonToPie(build, &model, data);
  // }

  // if (!model.is_skinned) // apply transformations directly to rigid mesh
  // {
  //   if (!Mat4_IsIdentity(model.spec_transform))
  //   {
  //     ForU32(mesh_index, model.meshes_count)
  //     {
  //       BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
  //       ForU32(i, bk_mesh.verts_count)
  //       {
  //         V3 *position = (V3*)BK_BufferAtFloat(&bk_mesh.positions, i*3);
  //         V4 p = V4_From_XYZ_W(*position, 1.f);
  //         p = V4_MulM4(model.spec_transform, p);

  //         V3 *normal = (V3*)BK_BufferAtFloat(&bk_mesh.normals, i*3);
  //         V3 n = V3_Rotate(*normal, spec.rot);

  //         *position = V3_FromV4_XYZ(p);
  //         *normal = n;
  //       }
  //     }
  //   }
  // }

  // // Vertices export
  // BK_GLTF_ExportModelToPie(build, &model);

  // // Reset tmp arena
  // Arena_PopScope(scratch);
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

BK_GLTF_UnpackAccessor :: (accessor: *cgltf_accessor, buffer: [..] *$T) -> slice: [] T
{
  comp_count := cgltf_num_components(accessor.type);
  total_count := accessor.count * comp_count;
  comp_size := cgltf_component_size(accessor.component_type);

  if accessor.component_type == .cgltf_component_type_r_32f
  {
    Check(T == float);
    numbers_slice := array_add_many(buffer, total_count);
    unpacked_count := cgltf_accessor_unpack_floats(accessor, *numbers[0], total_count);
    Check(unpacked_count == total_count);
    return numbers_slice;
  }

  numbers_slice := array_add_many(buffer, total_count);

  if size_of(T) >= comp_size
  {
    unpacked_count := cgltf_accessor_unpack_indices(accessor, *numbers_slice[0], size_of(T), total_count);
    M_Check(unpacked_count == total_count);
  }
  else
  {
    // Alloc tmp buffer
    auto_release_temp();
    bytes_to_allocate := comp_size * total_count;
    tmp_nums := talloc(bytes_to_allocate);

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
      if buffer.elem_size == {
        case 1;
        Check(value <= U8_MAX);
        numbers_slice = cast,no_check(u8) value;
        case 2;
        Check(value <= U16_MAX);
        numbers_slice = cast,no_check(u16) value;
        case 4;
        Check(value <= U32_MAX);
        numbers_slice = cast,no_check(u32) value;
        case;
        Check(false, "Unsupported elem_size downconversion");
      }
    }
  }

  return numbers_slice;
}
