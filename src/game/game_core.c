//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static void Game_AnimateObjects()
{
  ForArray(obj_index, APP.all_objects)
  {
    Object *obj = APP.all_objects + obj_index;

    if (OBJ_HasAllFlags(obj, ObjFlag_AnimateRotation))
    {
      Quat q0 = obj->l.animated_rot;
      Quat q1 = obj->s.rotation;

      float w1 = APP.dt * 10.f;
      float w0 = 1.f - w1;

      if (Quat_Inner(q0, q1) < 0.f)
        w1 = -w1;

      obj->l.animated_rot = Quat_Normalize(Quat_Mix(q0, q1, w0, w1));
    }

    if (OBJ_HasAnyFlag(obj, ObjFlag_AnimatePosition))
    {
      V3 delta = V3_Sub(obj->s.p, obj->l.animated_p);
      float speed = APP.dt * 10.f;
      delta = V3_Scale(delta, speed);

      obj->l.animated_p = V3_Add(obj->l.animated_p, delta);
      ForArray(i, obj->l.animated_p.E)
      {
        if (AbsF(delta.E[i]) < 0.1f)
          obj->l.animated_p.E[i] = obj->s.p.E[i];
      }
    }

    if (OBJ_HasAnyFlag(obj, ObjFlag_AnimateT))
    {
      if (obj->s.animation_index == 23)
      {
        obj->l.animation_t += APP.dt;
      }
      else
      {
        float dist = V3_Length(obj->s.moved_dp);
        obj->l.animation_t += dist * 0.02f;
      }

      obj->l.animation_t = AN_WrapAnimationTime(&Worker_Skeleton, obj->s.animation_index, obj->l.animation_t);
    }
  }
}

static void Game_DrawObjects()
{
  ForArray(obj_index, APP.all_objects)
  {
    Object *obj = APP.all_objects + obj_index;

    if (OBJ_HasAnyFlag(obj, ObjFlag_DrawModel))
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

      if      (obj->s.model == MODEL_Teapot)
        RDR_AddRigid(RdrRigid_Teapot, transform, obj->s.color);
      else if (obj->s.model == MODEL_Flag)
        RDR_AddRigid(RdrRigid_Flag, transform, obj->s.color);
      else if (obj->s.model == MODEL_FemaleWorker)
        RDR_AddSkinned(RdrSkinned_Worker, transform, obj->s.color, obj->s.animation_index, obj->l.animation_t);
    }

    if (OBJ_HasAnyFlag(obj, ObjFlag_DrawCollision))
    {
      U32 face_count = 6;
      U32 vertices_per_face = 3*2;
      U32 vert_count = face_count * vertices_per_face;

      TEX_Kind tex_kind = Clamp(0, TEX_COUNT-1, obj->s.texture);
      RDR_WallVertex *wall_verts = RDR_PushWallVertices(tex_kind, vert_count);
      if (wall_verts)
      {
        ForU32(i, vert_count)
        {
          SDL_zerop(&wall_verts[i]);
          wall_verts[i].color = obj->s.color;
        }

        float bot_z = obj->s.p.z;
        float height = obj->s.collision_height;
        if (height <= 0.f)
        {
          height = 10.f;
          bot_z -= height; // @todo do not draw floor/walls if height == 0
        }
        float top_z = bot_z + height;

        CollisionVertices collision = obj->s.collision.verts;
        {
          U32 cube_index_map[] =
          {
            4,5,0,5,1,0, // E
            6,7,2,7,3,2, // W
            5,6,1,6,2,1, // N
            7,4,3,4,0,3, // S
            5,4,6,4,7,6, // Top
            2,3,1,3,0,1, // Bottom
          };
          Assert(ArrayCount(cube_index_map) == vert_count);

          V3 cube_verts[8] =
          {
            V3_From_XY_Z(collision.arr[0], bot_z), // 0
            V3_From_XY_Z(collision.arr[1], bot_z), // 1
            V3_From_XY_Z(collision.arr[2], bot_z), // 2
            V3_From_XY_Z(collision.arr[3], bot_z), // 3
            V3_From_XY_Z(collision.arr[0], top_z), // 4
            V3_From_XY_Z(collision.arr[1], top_z), // 5
            V3_From_XY_Z(collision.arr[2], top_z), // 6
            V3_From_XY_Z(collision.arr[3], top_z), // 7
          };

          ForArray(i, cube_index_map)
          {
            AssertBounds(i, cube_index_map);
            U32 index = cube_index_map[i];

            AssertBounds(index, cube_verts);
            wall_verts[i].p = cube_verts[index];
            wall_verts[i].p.x += obj->s.p.x;
            wall_verts[i].p.y += obj->s.p.y;
          }

          {
            V4 test = {};
            test.x = wall_verts[0].p.x;
            test.y = wall_verts[0].p.y;
            test.z = wall_verts[0].p.z;
            test.w = 1.f;

            V4 post_view = V4_MulM4(APP.camera_all_mat, test);

            V4 div = post_view;
            div.x /= div.w;
            div.y /= div.w;
            div.z /= div.w;
            div.w /= div.w;
          }
        }

        float w0 = V2_Length(V2_Sub(collision.arr[0], collision.arr[1]));
        float w1 = V2_Length(V2_Sub(collision.arr[1], collision.arr[2]));
        float w2 = V2_Length(V2_Sub(collision.arr[2], collision.arr[3]));
        float w3 = V2_Length(V2_Sub(collision.arr[3], collision.arr[0]));

        float texels_per_cm = obj->s.texture_texels_per_cm;
        if (texels_per_cm <= 0.f)
          texels_per_cm = 0.015f;

        ForU32(face_i, face_count)
        {
          // face texture UVs
          V2 face_dim = {};
          switch (face_i)
          {
            case WorldDir_E: face_dim = (V2){w0, height}; break;
            case WorldDir_W: face_dim = (V2){w2, height}; break;
            case WorldDir_N: face_dim = (V2){w3, height}; break;
            case WorldDir_S: face_dim = (V2){w1, height}; break;
            case WorldDir_T: face_dim = (V2){w0, w1}; break; // works for rects only
            case WorldDir_B: face_dim = (V2){w0, w1}; break; // works for rects only
          }

          face_dim = V2_Scale(face_dim, texels_per_cm);
          V2 face_uvs[6] =
          {
            (V2){0, 0},
            (V2){face_dim.x, 0},
            (V2){0, face_dim.y},
            (V2){face_dim.x, 0},
            (V2){face_dim.x, face_dim.y},
            (V2){0, face_dim.y},
          };

          // face normals @todo CLEAN THIS UP
          Quat normal_rot = Quat_Identity();
          switch (face_i)
          {
            case WorldDir_E:
            case WorldDir_W:
            case WorldDir_N:
            case WorldDir_S: normal_rot = Quat_FromAxisAngle_RH(Axis3_Y(), -0.25f); break;
            case WorldDir_B: normal_rot = Quat_FromAxisAngle_RH(Axis3_Y(), 0.5f); break;
          }

          switch (face_i)
          {
            case WorldDir_E: normal_rot = Quat_Mul(normal_rot, Quat_FromAxisAngle_RH(Axis3_X(), 0.5f)); break;
            case WorldDir_N: normal_rot = Quat_Mul(normal_rot, Quat_FromAxisAngle_RH(Axis3_X(), -0.25f)); break;
            case WorldDir_S: normal_rot = Quat_Mul(normal_rot, Quat_FromAxisAngle_RH(Axis3_X(), 0.25f)); break;
          }

          V3 ideal_E = (V3){1,0,0};
          V3 ideal_W = (V3){-1,0,0};
          V3 ideal_N = (V3){0,1,0};
          V3 ideal_S = (V3){0,-1,0};

          V3 obj_norm_E = V3_From_XY_Z(obj->s.collision.norms.arr[0], 0);
          V3 obj_norm_W = V3_From_XY_Z(obj->s.collision.norms.arr[2], 0);
          V3 obj_norm_N = V3_From_XY_Z(obj->s.collision.norms.arr[1], 0);
          V3 obj_norm_S = V3_From_XY_Z(obj->s.collision.norms.arr[3], 0);

          Quat correction_E = Quat_FromNormalizedPair(ideal_E, obj_norm_E);
          Quat correction_W = Quat_FromNormalizedPair(ideal_W, obj_norm_W);
          Quat correction_N = Quat_FromNormalizedPair(ideal_N, obj_norm_N);
          Quat correction_S = Quat_FromNormalizedPair(ideal_S, obj_norm_S);

          switch (face_i)
          {
            case WorldDir_E: normal_rot = Quat_Mul(correction_E, normal_rot); break;
            case WorldDir_W: normal_rot = Quat_Mul(correction_W, normal_rot); break;
            case WorldDir_N: normal_rot = Quat_Mul(correction_N, normal_rot); break;
            case WorldDir_S: normal_rot = Quat_Mul(correction_S, normal_rot); break;
          }

          ForU32(vert_i, vertices_per_face)
          {
            U32 i = (face_i * vertices_per_face) + vert_i;
            wall_verts[i].uv = face_uvs[vert_i];
            wall_verts[i].normal_rot = normal_rot;
          }
        }
      }
    }
  }
}

static void Game_SetWindowPosSize(I32 px, I32 py, I32 w, I32 h)
{
  SDL_SetWindowPosition(APP.window, px, py);
  SDL_SetWindowSize(APP.window, w, h);
}

static void Game_AutoLayoutApply(U32 user_count,
                                 I32 px, I32 py, I32 w, I32 h)
{
  if (user_count == APP.latest_autolayout_user_count)
    return;

  APP.latest_autolayout_user_count = user_count;
  Game_SetWindowPosSize(px, py, w, h);
}

static void Game_Iterate()
{
  // pre-frame setup
  {
    APP.frame_id += 1;

    U64 new_frame_time = SDL_GetTicks();
    U64 delta_time = new_frame_time - APP.frame_time;
    APP.frame_time = new_frame_time;
    APP.dt = delta_time * (0.001f);
    APP.dt = Min(APP.dt, 1.f / 16.f); // clamp dt to 16 fps
    APP.tick_dt_accumulator += APP.dt;

    if (APP.debug.fixed_dt)
    {
      APP.dt = APP.debug.fixed_dt;
    }

    APP.at = WrapF(0.f, 1000.f, APP.at + APP.dt);
  }

  NET_IterateReceive();

  if (APP.debug.single_tick_stepping)
  {
    if (APP.debug.unpause_one_tick)
    {
      APP.tick_id += 1;
      TICK_Iterate();
      APP.debug.unpause_one_tick = false;
    }
  }
  else
  {
    while (APP.tick_dt_accumulator > TIME_STEP)
    {
      APP.tick_id += 1;
      APP.tick_dt_accumulator -= TIME_STEP;
      TICK_Iterate();
    }
  }

  NET_IterateTimeoutUsers();
  NET_IterateSend();

  if (KEY_Pressed(SDL_SCANCODE_RETURN))
    APP.debug.noclip_camera = !APP.debug.noclip_camera;

  // move camera; calculate camera scale
  if (APP.debug.noclip_camera)
  {
    if (KEY_Held(KEY_MouseLeft))
    {
      float rot_speed = 0.05f * APP.dt;
      APP.camera_angles.z += rot_speed * APP.mouse_delta.x;
      APP.camera_angles.y += rot_speed * APP.mouse_delta.y * (-2.f / 3.f);
      APP.camera_angles.z = WrapF(-0.5f, 0.5f, APP.camera_angles.z);
      APP.camera_angles.y = Clamp(-0.2f, 0.2f, APP.camera_angles.y);
    }

    V3 move_dir = {0};
    if (KEY_Held(SDL_SCANCODE_UP))    move_dir.x += 1;
    if (KEY_Held(SDL_SCANCODE_DOWN))  move_dir.x -= 1;
    if (KEY_Held(SDL_SCANCODE_LEFT))  move_dir.y += 1;
    if (KEY_Held(SDL_SCANCODE_RIGHT)) move_dir.y -= 1;
    if (KEY_Held(SDL_SCANCODE_SPACE)) move_dir.z += 1;
    if (KEY_Held(SDL_SCANCODE_LSHIFT) || KEY_Held(SDL_SCANCODE_RSHIFT)) move_dir.z -= 1;
    move_dir = V3_Normalize(move_dir);
    move_dir = V3_Rotate(move_dir, Quat_FromAxisAngle_RH((V3){0,0,-1} /* @todo figure out why -1 here fixes things */, APP.camera_angles.z));
    move_dir = V3_Scale(move_dir, APP.dt * 200.f);
    APP.camera_p = V3_Add(APP.camera_p, move_dir);
  }
  else
  {
    Object *player = OBJ_Get(APP.client.player_key, ObjStorage_Net);
    if (!OBJ_IsNil(player))
    {
      APP.camera_p = player->s.p;
      APP.camera_p.z += 130.f;
      APP.camera_p.x -= 60.f;
      APP.camera_angles = (V3){0, -0.15f, 0};
    }
  }

  // calculate camera transform matrices
  {
    APP.camera_move_mat = Mat4_InvTranslation(Mat4_Translation(APP.camera_p));

    APP.camera_rot = Quat_FromAxisAngle_RH((V3){1,0,0}, APP.camera_angles.x);
    APP.camera_rot = Quat_Mul(Quat_FromAxisAngle_RH((V3){0,0,1}, APP.camera_angles.z), APP.camera_rot);
    APP.camera_rot = Quat_Mul(Quat_FromAxisAngle_RH((V3){0,1,0}, APP.camera_angles.y), APP.camera_rot);

#if 0
    APP.camera_perspective_mat =
      Mat4_Perspective(APP.camera_fov_y, (float)APP.window_width/APP.window_height, 2.f, 4000.f);
#else
    float scale = 0.4f;
    float w = APP.window_width * 0.5f * scale;
    float h = APP.window_height * 0.5f * scale;
    APP.camera_perspective_mat =
      Mat4_Orthographic(-w, w, -h, h, 2.f, 4000.f);
#endif

    Mat4 rot_mat = Mat4_Rotation_Quat(APP.camera_rot);
    APP.camera_all_mat = Mat4_Mul(APP.camera_perspective_mat, Mat4_Mul(rot_mat, APP.camera_move_mat));
  }

  // calculate mouse in screen space -> mouse in world space (at Z == 0)
  {
    V2 view_mouse =
    {
      (APP.mouse.x / (float)APP.window_width)  * 2.f - 1.f,
      (APP.mouse.y / (float)APP.window_height) * 2.f - 1.f,
    };

    V3 plane_origin = {};
    V3 plane_normal = {0,0,1};

    float aspect = (float)APP.window_width / APP.window_height;
    float tan = TanF(APP.camera_fov_y * 0.5f);
    aspect *= aspect;
    tan    *= tan;

    float x = tan * view_mouse.x * aspect;
    float y = tan * view_mouse.y;
    V3 aim_dir =
    {
      x * APP.camera_all_mat.elem[0][0] + y * APP.camera_all_mat.elem[0][1] + APP.camera_all_mat.elem[0][2],
      x * APP.camera_all_mat.elem[1][0] + y * APP.camera_all_mat.elem[1][1] + APP.camera_all_mat.elem[1][2],
      x * APP.camera_all_mat.elem[2][0] + y * APP.camera_all_mat.elem[2][1] + APP.camera_all_mat.elem[2][2],
    };

    float t_numerator = V3_Inner(V3_Sub(plane_origin, APP.camera_p), plane_normal);
    float t_denominator = V3_Inner(aim_dir, plane_normal);

    APP.world_mouse_valid = t_denominator < 0.f;
    if (APP.world_mouse_valid)
    {
      float t = t_numerator / t_denominator;
      V3 world_off = V3_Scale(aim_dir, t);
      V3 world_mouse = V3_Add(APP.camera_p, world_off);
      APP.world_mouse = (V2){world_mouse.x, world_mouse.y};
    }
  }

  // move sun
  {
    float sun_x = SinF(APP.at * 0.03f);
    float sun_y = CosF(APP.at * 0.03f);
    APP.towards_sun_dir = V3_Normalize((V3){sun_x, sun_y, 1.f});

    V3 sun_dist = V3_Scale(APP.towards_sun_dir, 900.f);
    V3 sun_obj_p = V3_Add(APP.camera_p, sun_dist);
    OBJ_Get(APP.sun, ObjStorage_Local)->s.p = sun_obj_p;
  }

  Game_AnimateObjects();

  Object *marker = OBJ_Get(APP.pathing_marker, ObjStorage_Local);
  if (!OBJ_IsNil(marker))
  {
    if (KEY_Held(SDL_SCANCODE_W) ||
        KEY_Held(SDL_SCANCODE_S) ||
        KEY_Held(SDL_SCANCODE_A) ||
        KEY_Held(SDL_SCANCODE_D))
    {
      marker->s.p.z = 400.f;
    }

    if (KEY_Held(KEY_MouseRight) && APP.world_mouse_valid)
    {
      marker->s.flags |= ObjFlag_DrawModel;
      marker->s.model = MODEL_Flag;
      marker->s.p = V3_From_XY_Z(APP.world_mouse, 0);

      marker->l.animated_p.x = marker->s.p.x;
      marker->l.animated_p.y = marker->s.p.y;
      if (KEY_Pressed(KEY_MouseRight))
        marker->l.animated_p.z = 50.f;

      APP.pathing_marker_set = true;
    }
  }

  Game_DrawObjects();
}

static void Game_Init()
{
  // init debug options
  {
    //APP.debug.fixed_dt = 0.1f;
    //APP.debug.single_tick_stepping = true;
    //APP.debug.noclip_camera = true;
    APP.debug.draw_collision_box = true;
    APP.log_filter &= ~(LogFlags_NetDatagram);
  }

  NET_Init();

  APP.frame_time = SDL_GetTicks();
  APP.camera_fov_y = 0.19f;
  APP.camera_p = (V3){-50.f, 0.f, 70.f};
  APP.camera_angles = (V3){0, -0.05f, 0};
  APP.obj_serial_counter = 1;
  APP.tick_id = NET_CLIENT_MAX_SNAPSHOTS;

  // add walls, ground etc
  {
    float thickness = 20.f;
    float length = 400.f;
    float off = length*0.5f - thickness*0.5f;
    OBJ_CreateWall((V2){off, 0}, (V2){thickness, length});
    OBJ_CreateWall((V2){-off, 0}, (V2){thickness, length});
    OBJ_CreateWall((V2){0, off}, (V2){length, thickness});
    OBJ_CreateWall((V2){0,-off}, (V2){length*0.5f, thickness});

    {
      Object *rotated_wall = OBJ_CreateWall((V2){0.75f*off, -0.5f*off},
                                            (V2){thickness, 5.f*thickness});

      Vertices_Rotate(rotated_wall->s.collision.verts.arr,
                      ArrayCount(rotated_wall->s.collision.verts.arr),
                      0.1f);

      Collision_RecalculateNormals(&rotated_wall->s.collision);
    }

    {
      Object *ground = OBJ_Create(ObjStorage_Local, ObjFlag_DrawCollision);
      ground->s.collision.verts = CollisionVertices_FromRectDim((V2){4000, 4000});
      Collision_RecalculateNormals(&ground->s.collision);
      ground->s.texture = TEX_PavingStones067;
    }

    {
      Object *flying_cube = OBJ_Create(ObjStorage_Local, ObjFlag_DrawCollision);
      flying_cube->s.p = (V3){70, 70, 70};
      flying_cube->s.collision_height = 50;
      flying_cube->s.texture = TEX_Tiles101;
      flying_cube->s.texture_texels_per_cm = 0.05f;
      flying_cube->s.collision.verts = CollisionVertices_FromRectDim((V2){50, 50});
      Collision_RecalculateNormals(&flying_cube->s.collision);
    }

    {
      Object *sun = OBJ_Create(ObjStorage_Local, ObjFlag_DrawCollision);
      sun->s.texture = TEX_Leather011;
      sun->s.collision_height = 50;
      sun->s.collision.verts = CollisionVertices_FromRectDim((V2){50, 50});
      Collision_RecalculateNormals(&sun->s.collision);
      APP.sun = sun->s.key;
    }
  }

  // pathing marker
  {
    APP.pathing_marker = OBJ_Create(ObjStorage_Local, ObjFlag_AnimatePosition)->s.key;
  }
}
