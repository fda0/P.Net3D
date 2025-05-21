static void WORLD_DrawObjects()
{
  ForArray(obj_index, APP.all_objects)
  {
    Object *obj = APP.all_objects + obj_index;

    bool draw_model = OBJ_HasAnyFlag(obj, ObjFlag_DrawModel);
    bool draw_collision = (OBJ_HasAnyFlag(obj, ObjFlag_DrawCollision));
    bool draw_model_collision = (draw_model && APP.debug.draw_model_collision);

    if (draw_model)
    {
      V3 pos = obj->s.p;
      if (OBJ_HasAnyFlag(obj, ObjFlag_AnimatePosition))
        pos = obj->l.animated_p;

      Mat4 transform = Mat4_Translation(pos);
      if (OBJ_HasAnyFlag(obj, ObjFlag_AnimateRotation))
      {
        Mat4 rot_mat = Mat4_Rotation_Quat(obj->l.animated_rot);
        transform = Mat4_Mul(transform, rot_mat); // rotate first, translate second
      }

      WORLD_DrawModel(obj->s.model, transform, obj->s.color, obj->s.animation_index, obj->l.animation_t);
    }

    if (draw_collision || draw_model_collision)
    {
      float height = obj->s.height;
      MATERIAL_Key material = obj->s.material;

      if (draw_model_collision)
      {
        if (!height) height = 0.2f;
        if (MATERIAL_KeyIsZero(material)) material = MATERIAL_CreateKey(S8Lit("tex.Leather011"));
      }

      U32 face_count = (height ? 6 : 1);
      U32 vertices_per_face = 3*2;
      U32 mesh_verts_count = face_count * vertices_per_face;

      WORLD_Vertex mesh_verts[6 * 3 * 2]; // CPU side temporary buffer
      Memclear(mesh_verts, sizeof(mesh_verts));
      Assert(ArrayCount(mesh_verts) >= mesh_verts_count);

      float bot_z = obj->s.p.z;
      float top_z = bot_z + height;

      OBJ_Collider collision = obj->s.collider_vertices;
      {
        V3 cube_verts[8] =
        {
          V3_From_XY_Z(collision.vals[0], bot_z), // 0
          V3_From_XY_Z(collision.vals[1], bot_z), // 1
          V3_From_XY_Z(collision.vals[2], bot_z), // 2
          V3_From_XY_Z(collision.vals[3], bot_z), // 3
          V3_From_XY_Z(collision.vals[0], top_z), // 4
          V3_From_XY_Z(collision.vals[1], top_z), // 5
          V3_From_XY_Z(collision.vals[2], top_z), // 6
          V3_From_XY_Z(collision.vals[3], top_z), // 7
        };

        // mapping to expand verts to walls (each wall is made out of 2 triangles)
        U32 cube_verts_map_array[] =
        {
          0,5,4,0,1,5, // E
          2,7,6,2,3,7, // W
          1,6,5,1,2,6, // N
          3,4,7,3,0,4, // S
          6,4,5,6,7,4, // Top
          1,3,2,1,0,3, // Bottom
        };
        Assert(mesh_verts_count <= ArrayCount(cube_verts_map_array));

        U32 *cube_verts_map = cube_verts_map_array;
        if (face_count == 1) // generate top mesh only
          cube_verts_map = cube_verts_map_array + 6*WorldDir_T;

        ForU32(mesh_index, mesh_verts_count)
        {
          U32 cube_index = cube_verts_map[mesh_index];
          AssertBounds(cube_index, cube_verts);
          mesh_verts[mesh_index].p = cube_verts[cube_index];
          mesh_verts[mesh_index].p.x += obj->s.p.x;
          mesh_verts[mesh_index].p.y += obj->s.p.y;
        }
      }

      float w0 = V2_Length(V2_Sub(collision.vals[0], collision.vals[1]));
      float w1 = V2_Length(V2_Sub(collision.vals[1], collision.vals[2]));
      float w2 = V2_Length(V2_Sub(collision.vals[2], collision.vals[3]));
      float w3 = V2_Length(V2_Sub(collision.vals[3], collision.vals[0]));

      ForU32(face_i, face_count)
      {
        WorldDir face_dir = face_i;
        if (face_count == 1) face_dir = WorldDir_T;

        V2 face_uvs[6] =
        {
          (V2){0, 1},
          (V2){1, 0},
          (V2){0, 0},
          (V2){0, 1},
          (V2){1, 1},
          (V2){1, 0},
        };

        // If texture_texels_per_m is set - scale texture uvs
        if (obj->s.texture_texels_per_m)
        {
          // face texture UVs
          V2 face_dim = {};
          switch (face_dir)
          {
            default: break;
            case WorldDir_E: face_dim = (V2){w0, height}; break;
            case WorldDir_W: face_dim = (V2){w2, height}; break;
            case WorldDir_N: face_dim = (V2){w3, height}; break;
            case WorldDir_S: face_dim = (V2){w1, height}; break;
            case WorldDir_T: face_dim = (V2){w0, w1}; break; // works for rects only
            case WorldDir_B: face_dim = (V2){w0, w1}; break; // works for rects only
          }

          V2 face_scale = V2_Scale(face_dim, obj->s.texture_texels_per_m);
          ForArray(i, face_uvs)
          {
            face_uvs[i] = V2_Mul(face_uvs[i], face_scale);
          }
        }

        OBJ_UpdateColliderNormals(obj);
        V3 normal = {};
        V3 obj_norm_E = V3_From_XY_Z(obj->l.collider_normals.vals[0], 0);
        V3 obj_norm_W = V3_From_XY_Z(obj->l.collider_normals.vals[2], 0);
        V3 obj_norm_N = V3_From_XY_Z(obj->l.collider_normals.vals[1], 0);
        V3 obj_norm_S = V3_From_XY_Z(obj->l.collider_normals.vals[3], 0);
        switch (face_dir)
        {
          default: break;
          case WorldDir_E: normal = obj_norm_E; break;
          case WorldDir_W: normal = obj_norm_W; break;
          case WorldDir_N: normal = obj_norm_N; break;
          case WorldDir_S: normal = obj_norm_S; break;
          case WorldDir_T: normal = (V3){0,0,1}; break;
          case WorldDir_B: normal = (V3){0,0,-1}; break;
        }

        ForU32(vert_i, vertices_per_face)
        {
          U32 i = (face_i * vertices_per_face) + vert_i;
          mesh_verts[i].uv = face_uvs[vert_i];
          mesh_verts[i].normal = normal;
        }
      }

      // Transfer verts to GPU
      WORLD_DrawVertices(material, mesh_verts, mesh_verts_count);
    }
  }
}
