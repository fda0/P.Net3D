//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static bool LOG_Check(LOG_Category category)
{
  bool print = !!(category & APP.log_category_filter);
  return print;
}

static void GAME_AnimateObjects()
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
      V3 move_by = V3_Scale(delta, speed);
      obj->l.animated_p = V3_Add(obj->l.animated_p, move_by);

      ForArray(i, obj->l.animated_p.E)
      {
        if (FAbs(delta.E[i]) < 0.01f)
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
        obj->l.animation_t += dist * 0.016f * TICK_RATE; // @todo make this TICK_RATE independent & smooth across small and big TICK_RATEs
      }

      ASSET_Model *model = ASSET_GetModel(obj->s.model);
      ASSET_Skeleton *skel = ASSET_GetSkeleton(model->skeleton_index);
      obj->l.animation_t = ANIM_WrapAnimationTime(skel, obj->s.animation_index, obj->l.animation_t);
    }
  }
}

static void GAME_DrawObjects()
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

  // UI wip experiment
  if (0)
  {
    float full_dim = Min(APP.window_dim.x, APP.window_dim.y);
    float dim = full_dim*0.5f;

    ForU32(layer_index, 4)
    {
      float alpha = (layer_index == APP.font.active_layer ? 1.f : 0.6f);
      UI_Shape shape =
      {
        .color = Color32_RGBAf(1, 1, 1, alpha),
        .tex_min = (V2){0.f, 0.f},
        .tex_max = (V2){APP.font.texture_dim, APP.font.texture_dim},
        .tex_layer = layer_index,
      };
      if (layer_index & 2) shape.p_min.x += dim;
      if (layer_index & 1) shape.p_min.y += dim;
      shape.p_max = V2_Add(shape.p_min, (V2){dim, dim});
      UI_DrawRaw(shape);
    }
  }
}

static void GAME_SetWindowPosSize(I32 px, I32 py, I32 w, I32 h)
{
  SDL_SetWindowPosition(APP.window, px, py);
  SDL_SetWindowSize(APP.window, w, h);
}

static void GAME_AutoLayoutApply(U32 user_count,
                                 I32 px, I32 py, I32 w, I32 h)
{
  if (user_count == APP.latest_autolayout_user_count)
    return;

  APP.latest_autolayout_user_count = user_count;
  GAME_SetWindowPosSize(px, py, w, h);
}

static void GAME_Iterate()
{
  // pre-frame setup
  {
    APP.frame_id += 1;

    U64 new_frame_timestamp = SDL_GetTicks();
    U64 delta_timestamp = new_frame_timestamp - APP.timestamp;
    delta_timestamp = Min(delta_timestamp, 100); // clamp dt to 100ms / 10 fps

    APP.timestamp = new_frame_timestamp;
    APP.tick_timestamp_accumulator += delta_timestamp;
    APP.dt = delta_timestamp * (0.001f);

    if (APP.debug.fixed_dt)
      APP.dt = APP.debug.fixed_dt;

    APP.at = FWrap(0.f, 1000.f, APP.at + APP.dt);
  }

  GPU_ProcessWindowResize(false);
  FONT_ProcessWindowResize(false);
  UI_ProcessWindowAndFontResize();

  if (!APP.headless)
  {
    UI_StartFrame();
    UI_BuildUILayoutElements();
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
    static_assert((1000 % TICK_TIMESTAMP_STEP) == 0);
    static_assert((1000 / TICK_TIMESTAMP_STEP) == TICK_RATE);
    while (APP.tick_timestamp_accumulator > TICK_TIMESTAMP_STEP)
    {
      APP.tick_id += 1;
      APP.tick_timestamp_accumulator -= TICK_TIMESTAMP_STEP;
      TICK_Iterate();
    }
  }

  NET_IterateTimeoutUsers();
  NET_IterateSend();

  if (!APP.headless)
  {
    if (KEY_Pressed(SDL_SCANCODE_GRAVE))
      APP.debug.show_debug_window = !APP.debug.show_debug_window;

    if (KEY_Pressed(SDL_SCANCODE_RETURN))
      APP.debug.noclip_camera = !APP.debug.noclip_camera;

    if (KEY_Pressed(SDL_SCANCODE_BACKSPACE))
      APP.debug.sun_camera = !APP.debug.sun_camera;

    // move camera; calculate camera scale
    if (APP.debug.noclip_camera)
    {
      if (KEY_Held(KEY_MouseLeft) && !APP.debug.click_id)
      {
        float rot_speed = 0.0004f;
        APP.camera_angles.z += rot_speed * APP.mouse_delta.x;
        APP.camera_angles.y += rot_speed * APP.mouse_delta.y * (-2.f / 3.f);
        APP.camera_angles.z = FWrap(-0.5f, 0.5f, APP.camera_angles.z);
        APP.camera_angles.y = Clamp(-0.2f, 0.2f, APP.camera_angles.y);
      }

      V3 move_dir = {0};
      if (KEY_Held(SDL_SCANCODE_W)) move_dir.x += 1;
      if (KEY_Held(SDL_SCANCODE_S)) move_dir.x -= 1;
      if (KEY_Held(SDL_SCANCODE_A)) move_dir.y += 1;
      if (KEY_Held(SDL_SCANCODE_D)) move_dir.y -= 1;
      if (KEY_Held(SDL_SCANCODE_SPACE)) move_dir.z += 1;
      if (KEY_Held(SDL_SCANCODE_LSHIFT) || KEY_Held(SDL_SCANCODE_RSHIFT)) move_dir.z -= 1;
      move_dir = V3_Normalize(move_dir);
      move_dir = V3_Rotate(move_dir, Quat_FromAxisAngle_RH((V3){0,0,-1} /* @todo figure out why -1 here fixes things */, APP.camera_angles.z));
      move_dir = V3_Scale(move_dir, APP.dt * 2.f);
      APP.camera_p = V3_Add(APP.camera_p, move_dir);
    }
    else
    {
      Object *player = OBJ_Get(APP.client.player_key, OBJ_Network);
      if (!OBJ_IsNil(player))
      {
        APP.camera_p = player->s.p;
        APP.camera_p.z += 12.5f;
        APP.camera_p.x -= 9.8f;
        APP.camera_angles = (V3){0, -0.138f, 0};
      }
    }

    // Sun things
    {
      // Move sun
      float sun_x = FSin(0.5f + APP.at * 0.03f);
      float sun_y = FCos(0.5f + APP.at * 0.03f);
      V3 towards_sun_dir = V3_Normalize((V3){sun_x, sun_y, 2.f});
      APP.sun_dir = V3_Reverse(towards_sun_dir);

      V3 sun_dist = V3_Scale(towards_sun_dir, 40.f);
      V3 sun_obj_p = V3_Add(APP.camera_p, sun_dist);
      OBJ_Get(APP.sun, OBJ_Offline)->s.p = sun_obj_p;

      // Camera to sun
      APP.sun_camera_p = OBJ_GetAny(APP.sun)->s.p;
      Mat4 transl = Mat4_InvTranslation(Mat4_Translation(APP.sun_camera_p));

      float dim = 20.f;
      Mat4 projection = Mat4_Orthographic(-dim, dim, -dim, dim, 30.f, 100.f);
      Mat4 rot = Mat4_Rotation_Quat(Quat_FromPair(APP.sun_dir, AxisV3_X()));
      APP.sun_camera_transform = Mat4_Mul(projection, Mat4_Mul(rot, transl));
    }

    // calculate camera transform matrices
    {
      Mat4 transl = Mat4_InvTranslation(Mat4_Translation(APP.camera_p));
      Mat4 rot = Mat4_Rotation_RH((V3){1,0,0}, APP.camera_angles.x);
      rot = Mat4_Mul(Mat4_Rotation_RH((V3){0,0,1}, APP.camera_angles.z), rot);
      rot = Mat4_Mul(Mat4_Rotation_RH((V3){0,1,0}, APP.camera_angles.y), rot);
      Mat4 projection =
        Mat4_Perspective(APP.camera_fov_y, APP.window_dim.x/APP.window_dim.y, 0.25f, 25.f);

      APP.camera_transform = Mat4_Mul(projection, Mat4_Mul(rot, transl));
    }

    // calculate mouse in screen space -> mouse in world space (at Z == 0)
    {
      V2 view_mouse =
      {
        (APP.mouse.x / APP.window_dim.x)  * 2.f - 1.f,
        (APP.mouse.y / APP.window_dim.y) * -2.f + 1.f,
      };

      V3 plane_origin = {};
      V3 plane_normal = {0,0,1};

      float aspect = APP.window_dim.x / APP.window_dim.y;
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

    GAME_AnimateObjects();

    Object *marker = OBJ_Get(APP.pathing_marker, OBJ_Offline);
    if (!OBJ_IsNil(marker))
    {
      if (!APP.debug.noclip_camera &&
          (KEY_Held(SDL_SCANCODE_W) ||
           KEY_Held(SDL_SCANCODE_S) ||
           KEY_Held(SDL_SCANCODE_A) ||
           KEY_Held(SDL_SCANCODE_D)))
      {
        marker->s.p.z = -2.f;
      }

      if (KEY_Held(KEY_MouseRight) && APP.world_mouse_valid)
      {
        marker->s.flags |= ObjFlag_DrawModel;
        marker->s.model = MODEL_CreateKey(S8Lit("Flag"));
        marker->s.p = V3_From_XY_Z(APP.world_mouse, 0);

        marker->l.animated_p.x = marker->s.p.x;
        marker->l.animated_p.y = marker->s.p.y;
        if (KEY_Pressed(KEY_MouseRight))
          marker->l.animated_p.z = 0.5f;

        APP.pathing_marker_set = true;
      }
    }

    GAME_DrawObjects();
    UI_FinishFrame();
    GPU_Iterate();

    GPU_MEM_PostFrame();
    UI_PostFrame();
    ASSET_PostFrame();
  }

  // Serializations
  {
    SERIAL_DebugSettings(false);
    SERIAL_AssetSettings(false);
  }

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

static void GAME_Init()
{
  // init debug options
  APP.frame_id = 1000;

#if TESTS_ENABLED
  TEST_RunOnce();
#endif

  SERIAL_DebugSettings(true);
  NET_Init();

  if (!APP.headless)
  {
    GPU_Init();
    GPU_MEM_Init();
    ASSET_Init();
    SERIAL_AssetSettings(true);

    FONT_Init();
    ASSET_InitTextureThread();
    UI_Init();
  }

  APP.timestamp = SDL_GetTicks();
  APP.camera_fov_y = 0.111f;
  APP.camera_p = (V3){-4.f, 0.f, 1.f};
  APP.camera_angles = (V3){0, 0, 0};
  APP.obj_serial_counter = 1;
  APP.tick_id = NET_CLIENT_MAX_SNAPSHOTS;

  // Sun
  {
    Object *sun = OBJ_Create(OBJ_Offline, ObjFlag_DrawCollision);
    sun->s.material = MATERIAL_CreateKey(S8Lit("tex.Leather011"));
    sun->s.height = .5f;
    sun->s.collider_vertices = OBJ_GetColliderFromRect2D((V2){.5f, .5f});
    APP.sun = sun->s.key;
  }

  // Pathing marker
  {
    APP.pathing_marker = OBJ_Create(OBJ_Offline, ObjFlag_AnimatePosition)->s.key;
  }

  // Ground
  {
    Object *ground = OBJ_Create(OBJ_Offline, ObjFlag_DrawCollision);
    ground->s.collider_vertices = OBJ_GetColliderFromRect2D((V2){100, 100});
    ground->s.material = MATERIAL_CreateKey(S8Lit("tex.Grass004"));
    ground->s.texture_texels_per_m = 1.f;
  }

  {
    float thickness = 0.2f;
    float length = 8.f;
    float off = length*0.5f - thickness*0.5f;
    OBJ_CreateWall((V2){ off, 0}, (V2){thickness, length}, 0.8f);
    OBJ_CreateWall((V2){-off, 0}, (V2){thickness, length}, 2.f);
    OBJ_CreateWall((V2){0,  off}, (V2){length, thickness}, 1.6f);
    OBJ_CreateWall((V2){0, -off}, (V2){length*0.5f, thickness}, 1.2f);

    {
      Object *rotated_wall = OBJ_CreateWall((V2){0.75f*off, -0.5f*off},
                                            (V2){thickness, 5.f*thickness}, 1.f);
      OBJ_RotateColliderVertices(&rotated_wall->s.collider_vertices, 0.1f);
    }

    {
      Object *flying_cube = OBJ_Create(OBJ_Offline, ObjFlag_DrawCollision);
      flying_cube->s.p = (V3){0.5f, 1.2f, 1.f};
      flying_cube->s.material = MATERIAL_CreateKey(S8Lit("tex.Tiles101"));
      flying_cube->s.texture_texels_per_m = 1.f;
      flying_cube->s.height = 0.3f;
      flying_cube->s.collider_vertices = OBJ_GetColliderFromRect2D((V2){1.f, 1.f});
    }
  }

  // blocks thing
  {
    Object *thing = OBJ_Create(OBJ_Offline, ObjFlag_DrawCollision|ObjFlag_Collide);
    float d = 0.5f;
    thing->s.p = (V3){d,d,0};
    OBJ_SetColliderFromCube(thing, (V3){d,d,d});
    thing->s.material = MATERIAL_CreateKey(S8Lit("tex.Tiles087"));
  }

  // Add trees
  {
    U32 x_count = 16;
    U32 y_count = 16;

    ForU32(y, y_count)
    {
      ForU32(x, x_count)
      {
        U32 x_half_count = x_count/2;
        U32 y_half_count = y_count/2;

        V3 pos = {};
        if ((x^y)&1) pos.x = 4.70f;
        else         pos.y = 6.90f;

        pos.x += 4.0f*(x % x_half_count);
        pos.y += 2.4f*(y % y_half_count);

        pos.x *= (x < x_half_count ? -1.f : 1.f);
        pos.y *= (y < y_half_count ? -1.f : 1.f);

        Object *tree = OBJ_Create(OBJ_Offline, ObjFlag_DrawModel);
        if (!OBJ_IsNil(tree))
        {
          tree->s.p = pos;
          tree->s.model = MODEL_CreateKey(S8Lit("Tree"));
        }
      }
    }
  }
}
