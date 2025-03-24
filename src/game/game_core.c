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

      float w1 = Min(1.f, APP.dt * 16.f);
      float w0 = 1.f - w1;

      if (Quat_Dot(q0, q1) < 0.f)
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
        if (FAbs(delta.E[i]) < 0.1f)
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
            0,5,4,0,1,5, // E
            2,7,6,2,3,7, // W
            1,6,5,1,2,6, // N
            3,4,7,3,0,4, // S
            6,4,5,6,7,4, // Top
            1,3,2,1,0,3, // Bottom
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

            V4 post_view = V4_MulM4(APP.camera_transform, test);

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
            (V2){0, face_dim.y},
            (V2){face_dim.x, 0},
            (V2){0, 0},
            (V2){0, face_dim.y},
            (V2){face_dim.x, face_dim.y},
            (V2){face_dim.x, 0},
          };

          // face normals @todo CLEAN THIS UP
          Quat normal_rot = Quat_Identity();
          switch (face_i)
          {
            case WorldDir_E:
            case WorldDir_W:
            case WorldDir_N:
            case WorldDir_S: normal_rot = Quat_FromAxisAngle_RH(AxisV3_Y(), -0.25f); break;
            case WorldDir_B: normal_rot = Quat_FromAxisAngle_RH(AxisV3_Y(), 0.5f); break;
          }

          switch (face_i)
          {
            case WorldDir_E: normal_rot = Quat_Mul(normal_rot, Quat_FromAxisAngle_RH(AxisV3_X(), 0.5f)); break;
            case WorldDir_N: normal_rot = Quat_Mul(normal_rot, Quat_FromAxisAngle_RH(AxisV3_X(), -0.25f)); break;
            case WorldDir_S: normal_rot = Quat_Mul(normal_rot, Quat_FromAxisAngle_RH(AxisV3_X(), 0.25f)); break;
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

  // UI wip experiment
  {
    U32 shape_index = 0;
    U32 shape_encoded = (shape_index << 2);
    U32 encoded = shape_encoded;

    APP.rdr.ui.indices_count = 6;
    APP.rdr.ui.indices[0] = 0 | encoded;
    APP.rdr.ui.indices[1] = 1 | encoded;
    APP.rdr.ui.indices[2] = 2 | encoded;

    APP.rdr.ui.indices[3] = 2 | encoded;
    APP.rdr.ui.indices[4] = 1 | encoded;
    APP.rdr.ui.indices[5] = 3 | encoded;

    APP.rdr.ui.shapes_count = 1;
    APP.rdr.ui.shapes[0] = (UI_GpuShape)
    {
      .p_min = (V2){100.f, 50.f},
      .p_max = (V2){150.f, 300.f},
      .color = Color32_RGBf(0.7f, 0.9f, 0.6f),
    };

    APP.rdr.ui.clips_count = 1;
    APP.rdr.ui.clips[0] = (UI_GpuClip){};
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

    APP.at = FWrap(0.f, 1000.f, APP.at + APP.dt);
  }

  GPU_ProcessWindowResize(false);
  FA_ProcessWindowResize(false);

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

  if (KEY_Pressed(SDL_SCANCODE_BACKSPACE))
    APP.debug.sun_camera = !APP.debug.sun_camera;

  // move camera; calculate camera scale
  if (APP.debug.noclip_camera)
  {
    if (KEY_Held(KEY_MouseLeft))
    {
      float rot_speed = 0.05f * APP.dt;
      APP.camera_angles.z += rot_speed * APP.mouse_delta.x;
      APP.camera_angles.y += rot_speed * APP.mouse_delta.y * (-2.f / 3.f);
      APP.camera_angles.z = FWrap(-0.5f, 0.5f, APP.camera_angles.z);
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
      APP.camera_p.z += 250.f;
      APP.camera_p.x -= 140.f;
      APP.camera_angles = (V3){0, -0.155f, 0};
    }
  }

  // move sun
  {
    float sun_x = FSin(0.5f + APP.at * 0.03f);
    float sun_y = FCos(0.5f + APP.at * 0.03f);
    APP.towards_sun_dir = V3_Normalize((V3){sun_x, sun_y, 2.f});

    V3 sun_dist = V3_Scale(APP.towards_sun_dir, 900.f);
    V3 sun_obj_p = V3_Add(APP.camera_p, sun_dist);
    OBJ_Get(APP.sun, ObjStorage_Local)->s.p = sun_obj_p;
  }

  // camera to sun
  {
    APP.sun_camera_p = OBJ_GetAny(APP.sun)->s.p;
    Mat4 transl = Mat4_InvTranslation(Mat4_Translation(APP.sun_camera_p));

    V3 sun_dir = V3_Scale(APP.towards_sun_dir, -1.f);
    Mat4 rot = Mat4_Rotation_Quat(Quat_FromPair(sun_dir, AxisV3_X()));

    float scale = 0.8f;
    float w = APP.window_width * 0.5f * scale;
    float h = APP.window_height * 0.5f * scale;
    w = h = 700.f * scale;

    Mat4 projection = Mat4_Orthographic(-w, w, -h, h, 700.f, 2000.f);
    APP.sun_camera_transform = Mat4_Mul(projection, Mat4_Mul(rot, transl));
  }

  // calculate camera transform matrices
  {
    Mat4 transl = Mat4_InvTranslation(Mat4_Translation(APP.camera_p));
    Mat4 rot = Mat4_Rotation_RH((V3){1,0,0}, APP.camera_angles.x);
    rot = Mat4_Mul(Mat4_Rotation_RH((V3){0,0,1}, APP.camera_angles.z), rot);
    rot = Mat4_Mul(Mat4_Rotation_RH((V3){0,1,0}, APP.camera_angles.y), rot);
    Mat4 projection =
      Mat4_Perspective(APP.camera_fov_y, (float)APP.window_width/APP.window_height, 80.f, 2000.f);

    APP.camera_transform = Mat4_Mul(projection, Mat4_Mul(rot, transl));
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
    float tan = FTan(APP.camera_fov_y * 0.5f);
    aspect *= aspect;
    tan    *= tan;

    float x = tan * view_mouse.x * aspect;
    float y = tan * view_mouse.y;
    V3 aim_dir =
    {
      x * APP.camera_transform.elem[0][0] + y * APP.camera_transform.elem[0][1] + APP.camera_transform.elem[0][2],
      x * APP.camera_transform.elem[1][0] + y * APP.camera_transform.elem[1][1] + APP.camera_transform.elem[1][2],
      x * APP.camera_transform.elem[2][0] + y * APP.camera_transform.elem[2][1] + APP.camera_transform.elem[2][2],
    };

    float t_numerator = V3_Dot(V3_Sub(plane_origin, APP.camera_p), plane_normal);
    float t_denominator = V3_Dot(aim_dir, plane_normal);

    APP.world_mouse_valid = t_denominator < 0.f;
    if (APP.world_mouse_valid)
    {
      float t = t_numerator / t_denominator;
      V3 world_off = V3_Scale(aim_dir, t);
      V3 world_mouse = V3_Add(APP.camera_p, world_off);
      APP.world_mouse = (V2){world_mouse.x, world_mouse.y};
    }
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
      marker->s.p.z = -60.f;
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

  if (!APP.headless)
  {
    Game_DrawObjects();
    GPU_Iterate();
    RDR_PostFrameCleanup();
  }

  // Input cleanup
  APP.mouse_delta = (V2){};

  // Frame arena cleanup
  Arena_Reset(APP.a_frame, 0);
  Assert(APP.tmp->used == 0);

  if (APP.headless)
  {
    // Since we don't wait for v-sync in headless mode
    // we will wait here to avoid using 100% of the CPU.
    // This should be solved in a better way once
    // scheduling and multithreading is further along.
    SDL_Delay(16);
  }
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

  if (!APP.headless)
  {
    GPU_Init();
    FA_Init();
  }

  APP.frame_time = SDL_GetTicks();
  APP.camera_fov_y = 0.15f;
  APP.camera_p = (V3){-180.f, 0.f, 70.f};
  APP.camera_angles = (V3){0, 0, 0};
  APP.obj_serial_counter = 1;
  APP.tick_id = NET_CLIENT_MAX_SNAPSHOTS;

  // add walls, ground etc
  {
    float thickness = 20.f;
    float length = 400.f;
    float off = length*0.5f - thickness*0.5f;
    OBJ_CreateWall((V2){ off, 0}, (V2){thickness, length}, 70.f);
    OBJ_CreateWall((V2){-off, 0}, (V2){thickness, length}, 50.f);
    OBJ_CreateWall((V2){0,  off}, (V2){length, thickness}, 60.f);
    OBJ_CreateWall((V2){0, -off}, (V2){length*0.5f, thickness}, 40.f);

    {
      Object *rotated_wall = OBJ_CreateWall((V2){0.75f*off, -0.5f*off},
                                            (V2){thickness, 5.f*thickness}, 100.f);

      Vertices_Rotate(rotated_wall->s.collision.verts.arr,
                      ArrayCount(rotated_wall->s.collision.verts.arr),
                      0.1f);

      Collision_RecalculateNormals(&rotated_wall->s.collision);
    }

    {
      Object *ground = OBJ_Create(ObjStorage_Local, ObjFlag_DrawCollision);
      ground->s.collision.verts = CollisionVertices_FromRectDim((V2){4000, 4000});
      Collision_RecalculateNormals(&ground->s.collision);
      ground->s.texture = TEX_Grass004;
    }

    {
      Object *flying_cube = OBJ_Create(ObjStorage_Local, ObjFlag_DrawCollision);
      flying_cube->s.p = (V3){40, 40, 80};
      flying_cube->s.texture = TEX_Tiles101;
      flying_cube->s.texture_texels_per_cm = 0.018f;
      flying_cube->s.collision_height = 40;
      flying_cube->s.collision.verts = CollisionVertices_FromRectDim((V2){40, 40});
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

  FA_GlyphRun g0 = FA_GetGlyphRun(FA_Regular, S8Lit("hello world 🌍"));
  g0 = FA_GetGlyphRun(FA_Regular, S8Lit("hello sailor ⛵"));
}
