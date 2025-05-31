WORLD_Dir :: enum u32
{
  E; // +X
  W; // -X
  N; // +Y
  S; // -Y
  T; // +Z
  B; // -Z
  COUNT;
};

WORLD_ShaderFlags :: enum_flags u32 // @todo generate hlsl header for shaders out of this
{
  DoMeshSkinning;
  UseInstanceBuffer;
  SampleTexDiffuse;
  SampleTexNormal;
  SampleTexRoughness;
  ApplyShadows;
  DrawBorderAtUVEdge;
};

WORLD_Uniform :: struct
{
  camera_transform: Mat4;
  shadow_transform: Mat4;
  camera_position: V3 #align 16;
  sun_dir: V3 #align 16;
  flags: WORLD_ShaderFlags;

  fog_color: u32; // RGBA
  sky_ambient: u32; // RGBA
  sun_diffuse: u32; // RGBA
  sun_specular: u32; // RGBA

  material_diffuse: u32; // RGBA
  material_specular: u32; // RGBA
  material_roughness: float;
  material_loaded_t: float;
};

WORLD_Vertex :: struct
{
  p: V3;
  normal: V3;
  uv: V2;
  joints_packed4: u32;
  joint_weights: V4;
};

WORLD_InstanceModel :: struct
{
  transform: Mat4;
  color: u32;
  pose_offset: u32; // in indices; unused for rigid
};

WORLD_ApplyMaterialToUniform :: (uniform: *WORLD_Uniform, material: *ASSET_Material)
{
  uniform.material_loaded_t = material.stream.loaded_t;
  uniform.material_diffuse = material.params.diffuse;
  uniform.material_specular = material.params.specular;
  uniform.material_roughness = material.params.roughness;

  uniform.flags = .ApplyShadows;
  if material.has_texture && 0
  {
    if material.texture_layers >= 1  uniform.flags |= .SampleTexDiffuse;
    if material.texture_layers >= 2  uniform.flags |= .SampleTexNormal;
    if material.texture_layers >= 3  uniform.flags |= .SampleTexRoughness;
  }
}

WORLD_DrawModel :: (model_key: MODEL_Key, transform: Mat4, color: Color32, animation_index: u32, animation_t: float)
{
  instance := WORLD_InstanceModel.{transform = transform, color = color};
  model := ASSET_GetModel(model_key);
  if model.is_skinned
  {
    #if 0
    {
      instance.pose_offset = GPU_BATCH_GetPosesBatch().element_count;

      pose := ANIM_PoseFromAnimation(model.skeleton, animation_index, animation_t);
      GPU_MEM_TransferUploadBytes(GPU_MEM_GetPosesBatch(), pose.mats,
        pose.mats_count * sizeof(pose.mats[0]), pose.mats_count);
    }
  }

  instance_batch := GPU_BATCH_FindOrCreateBundle(.{type = .ModelInstances, model_key = model_key});
  GPU_BATCH_TransferUploadBytes(instance_batch, *instance, size_of(type_of(instance)), 1);
}

WORLD_DrawVertices :: (material: MATERIAL_Key, vertices: *WORLD_Vertex, vertices_count: u32)
{
  mesh_batch := GPU_BATCH_FindOrCreateBundle(.{type=.MeshVertices, material_key=material});
  transfer_size: u32 = vertices_count * size_of(type_of(vertices[0]));
  GPU_BATCH_TransferUploadBytes(mesh_batch, vertices, transfer_size, vertices_count);
}

WORLD_DrawObjects :: ()
{
  for *obj: G.obj.all_objects
  {
    draw_model := OBJ_HasAnyFlag(obj, .DRAW_MODEL);
    draw_collision := (OBJ_HasAnyFlag(obj, .DRAW_COLLISION));
    draw_model_collision := (draw_model && G.debug.draw_model_collision);

    if draw_model
    {
      pos := obj.s.p;
      if OBJ_HasAnyFlag(obj, .ANIMATE_POSITION)
        pos = obj.l.animated_p;

      transform := TranslationMatrix(pos);
      if (OBJ_HasAnyFlag(obj, .ANIMATE_ROTATION))
      {
        rot_mat := RotationMatrix(obj.l.animated_rot);
        transform = transform * rot_mat; // rotate first, translate second
      }

      WORLD_DrawModel(obj.s.model, transform, obj.s.color, obj.l.animation_index, obj.l.animation_t);
    }

    if draw_collision || draw_model_collision
    {
      height := obj.s.height;
      material := obj.s.material;

      if draw_model_collision
      {
        if !height  height = 0.2;
        if MATERIAL_IsZeroKey(material)  material = MATERIAL_CreateKey("tex.Leather011");
      }

      face_count := cast(u32) ifx height then 6 else 1;
      vertices_per_face: u32 = 3 * 2;
      mesh_verts_count: u32 = face_count * vertices_per_face;
      mesh_verts: [6 * 3 * 2] WORLD_Vertex; // CPU side temporary buffer

      bot_z := obj.s.p.z;
      top_z := bot_z + height;

      collision := obj.s.collider_vertices;
      {
        cube_verts := V3.[
          V3.{xy=collision.values[0], z=bot_z}, // 0
          V3.{xy=collision.values[1], z=bot_z}, // 1
          V3.{xy=collision.values[2], z=bot_z}, // 2
          V3.{xy=collision.values[3], z=bot_z}, // 3
          V3.{xy=collision.values[0], z=top_z}, // 4
          V3.{xy=collision.values[1], z=top_z}, // 5
          V3.{xy=collision.values[2], z=top_z}, // 6
          V3.{xy=collision.values[3], z=top_z}, // 7
        ];

        // mapping to expand verts to walls (each wall is made out of 2 triangles)
        cube_verts_map_array := u32.[
          0,5,4,0,1,5, // E
          2,7,6,2,3,7, // W
          1,6,5,1,2,6, // N
          3,4,7,3,0,4, // S
          6,4,5,6,7,4, // Top
          1,3,2,1,0,3, // Bottom
        ];

        cube_verts_map := *cube_verts_map_array[0];
        if (face_count == 1) // generate top mesh only
          cube_verts_map += 6*WORLD_Dir.T.(u32);

        for mesh_index: 0..mesh_verts_count-1
        {
          cube_index := cube_verts_map[mesh_index];
          mesh_verts[mesh_index].p = cube_verts[cube_index];
          mesh_verts[mesh_index].p.x += obj.s.p.x;
          mesh_verts[mesh_index].p.y += obj.s.p.y;
        }
      }

      w0 := length(collision.values[0] - collision.values[1]);
      w1 := length(collision.values[1] - collision.values[2]);
      w2 := length(collision.values[2] - collision.values[3]);
      w3 := length(collision.values[3] - collision.values[0]);

      for face_i: 0..face_count-1
      {
        face_dir := face_i.(WORLD_Dir);
        if (face_count == 1) face_dir = .T;

        face_uvs := V2.[
          .{0, 1},
          .{1, 0},
          .{0, 0},
          .{0, 1},
          .{1, 1},
          .{1, 0},
        ];

        // If texture_texels_per_m is set - scale texture uvs
        if (obj.s.texture_texels_per_m)
        {
          // face texture UVs
          face_dim: V2;
          if face_dir ==
          {
            case .E; face_dim = V2.{w0, height};
            case .W; face_dim = V2.{w2, height};
            case .N; face_dim = V2.{w3, height};
            case .S; face_dim = V2.{w1, height};
            case .T; face_dim = V2.{w0, w1}; // works for rects only
            case .B; face_dim = V2.{w0, w1}; // works for rects only
          }

          face_scale := face_dim * obj.s.texture_texels_per_m;
          for *face_uvs
            it.* *= face_scale;
        }

        OBJ_UpdateColliderNormals(obj);
        obj_norm_E := V3.{xy=obj.l.collider_normals.values[0]};
        obj_norm_W := V3.{xy=obj.l.collider_normals.values[2]};
        obj_norm_N := V3.{xy=obj.l.collider_normals.values[1]};
        obj_norm_S := V3.{xy=obj.l.collider_normals.values[3]};
        normal: V3;
        if face_dir ==
        {
          case .E; normal = obj_norm_E;
          case .W; normal = obj_norm_W;
          case .N; normal = obj_norm_N;
          case .S; normal = obj_norm_S;
          case .T; normal = V3.{0,0,1};
          case .B; normal = V3.{0,0,-1};
        }

        for vert_i: 0..vertices_per_face-1
        {
          i := (face_i * vertices_per_face) + vert_i;
          mesh_verts[i].uv = face_uvs[vert_i];
          mesh_verts[i].normal = normal;
        }
      }

      // Transfer verts to GPU
      WORLD_DrawVertices(material, *mesh_verts[0], mesh_verts_count);
    }
  }
}
