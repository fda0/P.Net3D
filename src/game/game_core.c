//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static bool LOG_Check(LOG_Category category)
{
  if (!category) category = LOG_Idk;

  bool print = !!(category & APP.log_category_filter);
  return print;
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
      move_dir = V3_Scale(move_dir, APP.dt * 4.f);
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
      float sun_time = APP.at*4.f;
      sun_time = 6.f;

      float sun_x = FSin(0.5f + sun_time * 0.03f);
      float sun_y = FCos(0.5f + sun_time * 0.03f);
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

    ANIM_AnimateObjects();

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

    WORLD_DrawObjects();
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

    ForU32(wall, 4)
    {
      U32 len = 32;
      ForU32(i, len)
      {
        float d = 0.5f;
        U32 x = (wall <= 1 ? i : (wall == 2 ? 0 : len));
        U32 y = (wall >= 2 ? i : (wall == 0 ? 0 : len));

        Object *thing = OBJ_Create(OBJ_Offline, ObjFlag_DrawCollision|ObjFlag_Collide);
        thing->s.p = (V3){d*x - len*0.5f*d, d*y - len*0.5f*d, 0};
        OBJ_SetColliderFromCube(thing, (V3){d,d,d});
        thing->s.material = MATERIAL_CreateKey(S8Lit("tex.Clay002"));
      }
    }
  }

  // Add trees
  {
    U32 x_count = 8;
    U32 y_count = 8;

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
