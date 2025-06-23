WORLD_DrawObjects :: ()
{
  for *obj: G.obj.all_objects
  {
    draw_model := OBJ_HasAnyFlag(obj, .DRAW_MODEL);
    draw_colliders := OBJ_HasAnyFlag(obj, .DRAW_COLLIDERS);
    debug_colliders := G.ui.dev.show_colliders;

    if draw_model
    {
      pos := obj.s.p;
      if OBJ_HasAnyFlag(obj, .ANIMATE_POSITION)
        pos = obj.l.animated_p;

      transform := TranslationMatrix(pos);
      if OBJ_HasAnyFlag(obj, .ANIMATE_ROTATION)
      {
        rot_mat := RotationMatrix(obj.l.animated_rot);
        transform = transform * rot_mat; // rotate first, translate second
      }

      WORLD_DrawModel(obj.s.model, transform, obj.s.color, obj.l.animation_index, obj.l.animation_t);
    }

    if draw_colliders || debug_colliders
    {
      height := obj.s.height;
      material := obj.s.material;

      if debug_colliders && draw_model
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

      collider := obj.s.collider;
      {
        cube_verts := V3.[
          V3.{xy=collider.vertices[0], z=bot_z}, // 0
          V3.{xy=collider.vertices[1], z=bot_z}, // 1
          V3.{xy=collider.vertices[2], z=bot_z}, // 2
          V3.{xy=collider.vertices[3], z=bot_z}, // 3
          V3.{xy=collider.vertices[0], z=top_z}, // 4
          V3.{xy=collider.vertices[1], z=top_z}, // 5
          V3.{xy=collider.vertices[2], z=top_z}, // 6
          V3.{xy=collider.vertices[3], z=top_z}, // 7
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

        cube_verts_map := cube_verts_map_array.data;
        if (face_count == 1) // generate top mesh only
          cube_verts_map += 6*WORLD_Dir.T.(u32);

        for mesh_index: MakeRange(mesh_verts_count)
        {
          cube_index := cube_verts_map[mesh_index];
          mesh_verts[mesh_index].p = cube_verts[cube_index];
          mesh_verts[mesh_index].p.x += obj.s.p.x;
          mesh_verts[mesh_index].p.y += obj.s.p.y;
        }
      }

      w0 := length(collider.vertices[0] - collider.vertices[1]);
      w1 := length(collider.vertices[1] - collider.vertices[2]);
      w2 := length(collider.vertices[2] - collider.vertices[3]);
      w3 := length(collider.vertices[3] - collider.vertices[0]);

      for face_i: MakeRange(face_count)
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

        obj_norm_E := V3.{xy=collider.normals[0]};
        obj_norm_W := V3.{xy=collider.normals[2]};
        obj_norm_N := V3.{xy=collider.normals[1]};
        obj_norm_S := V3.{xy=collider.normals[3]};
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

        for vert_i: MakeRange(vertices_per_face)
        {
          i := (face_i * vertices_per_face) + vert_i;
          mesh_verts[i].uv = face_uvs[vert_i];
          mesh_verts[i].normal = normal;
        }
      }

      // Transfer verts to GPU
      WORLD_DrawVertices(material, mesh_verts.data, mesh_verts_count);
    }
  }
}
