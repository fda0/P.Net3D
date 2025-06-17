#import "gltf_parser";

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
  // Parse .gltf file & load buffers
  data := gltf_parse_file(file_path);
  gltf_load_buffers(*data);

  //
  // Prepare strings based on file_path
  //
  dir_path, file_name_no_ext, _, _ := path_decomp(file_path);

  // Collect data from .gltf file
  // @todo Arena_Scope scratch = Arena_PushScope(BAKER.tmp);
  max_indices := 1024*256;
  max_verts := 1024*128;

  model: BK_GLTF_Model;
  model.name = ifx spec.name then spec.name else file_name_no_ext;
  model.spec = spec;
  model.is_skinned = data.animations.count > 0;
  array_resize(*model.meshes, data.materials.count);

  for * bk_mesh: model.meshes
  {
    // Fetch material data
    gltf_material := data.materials[it_index];

    // Create material
    bk_mesh.material_index = B.pie.materials.count;
    {
      pie_material := array_add(*B.pie.materials);
    //   BK_SetDefaultsPIEMaterial(pie_material);

    //   // Fill material name
    //   {
    //     PIE_ListStart(*B.pie.file, &pie_material->name, TYPE_U8);
    //     Pr_S8(&B.pie.file, model.name);
    //     Pr_Cstr(&B.pie.file, ".m");
    //     Pr_U32(&B.pie.file, mesh_index);
    //     Pr_Cstr(&B.pie.file, ".");
    //     Pr_Cstr(&B.pie.file, gltf_material->name);
    //     PIE_ListEnd(&B.pie.file, &pie_material->name);
    //   }

    //   S8 diffuse_tex_path = {};
    //   S8 normal_tex_path = {};
    //   S8 specular_tex_path = {};

    //   if (gltf_material->normal_texture.texture)
    //   {
    //     const char *uri = gltf_material->normal_texture.texture->image->uri;
    //     normal_tex_path = S8_ConcatDirFile(scratch.a, dir_path, S8_FromCstr(uri));
    //   }

    //   if (gltf_material->has_pbr_metallic_roughness)
    //   {
    //     cgltf_pbr_metallic_roughness *metal = &gltf_material->pbr_metallic_roughness;
    //     pie_material->params.diffuse   = Color32_V4(*(V4 *)metal->base_color_factor);
    //     pie_material->params.roughness = metal->roughness_factor;
    //   }

    //   if (gltf_material->has_pbr_specular_glossiness)
    //   {
    //     cgltf_pbr_specular_glossiness *gloss = &gltf_material->pbr_specular_glossiness;
    //     pie_material->params.diffuse   = Color32_V4(*(V4 *)gloss->diffuse_factor);
    //     pie_material->params.specular  = Color32_V3(*(V3 *)gloss->specular_factor);
    //     pie_material->params.roughness = 1.f - gloss->glossiness_factor;

    //     if (gloss->diffuse_texture.texture)
    //     {
    //       const char *uri = gloss->diffuse_texture.texture->image->uri;
    //       diffuse_tex_path = S8_ConcatDirFile(scratch.a, dir_path, S8_FromCstr(uri));
    //     }

    //     if (gloss->specular_glossiness_texture.texture)
    //     {
    //       const char *uri = gloss->specular_glossiness_texture.texture->image->uri;
    //       specular_tex_path = S8_ConcatDirFile(scratch.a, dir_path, S8_FromCstr(uri));
    //     }
      }

    //   if (gltf_material->alpha_mode != cgltf_alpha_mode_opaque)
    //   pie_material->params.flags |= PIE_MaterialFlag_HasAlpha;

    //   // Prepare tex_paths array
    //   // These are expected to be in a fixed order for now
    //   // So if any in the chain is missing we have to drop all subsequent textures.
    //   S8 tex_paths[3] = {};
    //   U32 tex_paths_count = 0;

    //   if (diffuse_tex_path.size)
    //   tex_paths[tex_paths_count++] = diffuse_tex_path;

    //   if (tex_paths_count == 1 && normal_tex_path.size)
    //   tex_paths[tex_paths_count++] = normal_tex_path;

    //   if (tex_paths_count == 2 && specular_tex_path.size)
    //   tex_paths[tex_paths_count++] = specular_tex_path;

    //   if (tex_paths_count)
    //   BK_TEX_LoadPaths(pie_material, tex_paths, tex_paths_count);
    // }
  }

  // The hierarchy of .gltf files might be a bit confusing.
  // Division into meshes and primitives wasn't strictly
  // necessary (it could have been merged into one) but it is what it is.
  // ForU64(gltf_mesh_index, data->meshes_count)
  // {
  //   cgltf_mesh *gltf_mesh = data->meshes + gltf_mesh_index;

  //   ForU64(gltf_primitive_index, gltf_mesh->primitives_count)
  //   {
  //     cgltf_primitive *primitive = gltf_mesh->primitives + gltf_primitive_index;

  //     // Our internal groups things into common meshes based on common material.
  //     U32 mesh_index = BK_GLTF_FindMaterialIndex(data, primitive->material);
  //     BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;

  //     if (primitive->type != cgltf_primitive_type_triangles)
  //     {
  //       M_LOG(M_GLTFWarning,
  //         "[GLTF LOADER] Primitive with non triangle type (%u) skipped",
  //         primitive->type);
  //       continue;
  //     }

  //     M_Check(primitive->indices);
  //     {
  //       U64 index_start_offset = bk_mesh->positions.used/3;

  //       cgltf_accessor *accessor = primitive->indices;
  //       M_Check(accessor->type == cgltf_type_scalar);
  //       U16 *numbers = BK_GLTF_UnpackAccessor(accessor, &bk_mesh->indices);

  //       if (index_start_offset)
  //       {
  //         ForU64(i, accessor->count)
  //         {
  //           U32 value = numbers[i] + index_start_offset;
  //           M_Check(value <= U16_MAX);
  //           numbers[i] = value;
  //         }
  //       }
  //     }

  //     ForU64(attribute_index, primitive->attributes_count)
  //     {
  //       cgltf_attribute *attribute = primitive->attributes + attribute_index;
  //       cgltf_accessor *accessor = attribute->data;

  //       U64 comp_count = cgltf_num_components(accessor->type);
  //       BK_Buffer *save_buf = 0;

  //       switch (attribute->type)
  //       {
  //         case cgltf_attribute_type_position:
  //         {
  //           save_buf = &bk_mesh->positions;
  //           M_Check(comp_count == 3);
  //           M_Check(accessor->component_type == cgltf_component_type_r_32f);
  //         } break;

  //         case cgltf_attribute_type_normal:
  //         {
  //           save_buf = &bk_mesh->normals;
  //           M_Check(comp_count == 3);
  //           M_Check(accessor->component_type == cgltf_component_type_r_32f);
  //         } break;

  //         case cgltf_attribute_type_tangent:
  //         {
  //           M_LOG(M_GLTFDebug, "[GLTF LOADER] Attribute with type tangent skipped");
  //           continue;
  //         } break;

  //         case cgltf_attribute_type_texcoord:
  //         {
  //           save_buf = &bk_mesh->texcoords;
  //           M_Check(comp_count == 2);
  //           M_Check(accessor->component_type == cgltf_component_type_r_32f);
  //         } break;

  //         case cgltf_attribute_type_joints:
  //         {
  //           save_buf = &bk_mesh->joint_indices;
  //           M_Check(comp_count == 4);
  //           //M_Check(accessor->component_type == cgltf_component_type_r_8u);
  //         } break;

  //         case cgltf_attribute_type_weights:
  //         {
  //           save_buf = &bk_mesh->weights;
  //           M_Check(comp_count == 4);
  //           M_Check(accessor->component_type == cgltf_component_type_r_32f);
  //         } break;

  //         default:
  //         {
  //           M_LOG(M_GLTFWarning,
  //             "[GLTF LOADER] Attribute with unknown type (%u) skipped",
  //             attribute->type);
  //           continue;
  //         } break;
  //       }

  //       BK_GLTF_UnpackAccessor(accessor, save_buf);
  //     }
  //   }
  // }

  // //
  // //
  // //

  // // Validate buffer counts
  // ForU32(mesh_index, model.meshes_count)
  // {
  //   BK_GLTF_Mesh *bk_mesh = model.meshes + mesh_index;
  //   M_Check(bk_mesh->positions.used % 3 == 0);
  //   bk_mesh->verts_count = bk_mesh->positions.used / 3;

  //   M_Check(bk_mesh->normals.used % 3 == 0);
  //   M_Check(bk_mesh->verts_count == bk_mesh->normals.used / 3);

  //   if (model.is_skinned)
  //   {
  //     M_Check(bk_mesh->joint_indices.used % 4 == 0);
  //     M_Check(bk_mesh->verts_count == bk_mesh->joint_indices.used / 4);
  //     M_Check(bk_mesh->weights.used % 4 == 0);
  //     M_Check(bk_mesh->verts_count == bk_mesh->weights.used / 4);
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
  //     ForU32(i, bk_mesh->verts_count)
  //     {
  //       V3 p = *(V3*)BK_BufferAtFloat(&bk_mesh->positions, i*3);
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
  //   M_Check(data->skins_count == 1);
  //   M_Check(data->skins[0].joints_count <= U32_MAX);
  //   model.joints_count = (U32)data->skins[0].joints_count;
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
  //       ForU32(i, bk_mesh->verts_count)
  //       {
  //         V3 *position = (V3*)BK_BufferAtFloat(&bk_mesh->positions, i*3);
  //         V4 p = V4_From_XYZ_W(*position, 1.f);
  //         p = V4_MulM4(model.spec_transform, p);

  //         V3 *normal = (V3*)BK_BufferAtFloat(&bk_mesh->normals, i*3);
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
