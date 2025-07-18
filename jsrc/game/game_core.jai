G: GAME_State; // Global GAME_State variable

GAME_State :: struct
{
  // core game state
  in_shutdown: bool;
  frame_number: s64;
  frame_timestamp: TimestampMS;
  tick_number: u64 = NET_CLIENT_MAX_SNAPSHOTS;
  tick_timestamp_accumulator: TimestampMS;
  dt: float; // frame delta time
  at: float; // animation time

  // startup profiling
  time_launch: Apollo_Time;
  time_init_sdl: Apollo_Time;
  time_init_game: Apollo_Time;
  time_init_finished: Apollo_Time;
  time_finished_first_iterate: Apollo_Time;

  // component states
  gpu: GPU_State;
  ast: ASSET_State;
  audio: AUDIO_State;
  hr: HOT_RELOAD_State;
  font: FONT_State;
  obj: OBJ_State;
  anim: ANIMATION_State;
  net: NET_State;
  client: CLIENT_State;
  server: SERVER_State;
  ui: UI_State;
  dev: DEV_State;

  // SDL, window stuff
  window: *SDL_Window;
  dpi_scaling: float;
  headless: bool; // no window mode
  init_window_p: V2 = .{-1, -1};
  init_window_dim: V2 = .{-1, -1};
  window_dim: V2;
  window_resized: bool; // window resized on this frame
  window_fullscreen: bool;
  window_borderless: bool;
  window_on_top: bool;
  window_autolayout: bool;
  server_exit_on_disconnect: bool;
  sdl_is_fullscreen: bool;
  // SDL audio

  // user input
  mouse: V2;
  mouse_delta: V2;
  mouse_scroll: V2;
  world_mouse_valid: bool;
  world_mouse: V3;
  keys: [KEY_Code.COUNT] Key;
  hover_object: OBJ_Key;

  // gameplay actions
  action: Action;

  // sun
  sun_dir: V3; // normalized
  sun_camera_p: V3;
  sun_camera_transform: Mat4;

  // camera
  camera_p: V3 = .{-10, 0, 1};
  camera_angles: V3;
  camera_fov_y: float = 0.111;
  camera_transform: Mat4;
}

GAME_Init :: ()
{
  G.frame_timestamp = GetTime(.NOW);

  if !G.headless
  {
    ASSET_InitPreGPU();
    GPU_Init();
    GPU_BATCH_Init();
    ASSET_InitPostGPU();
    ANIMATION_Init();
    FONT_Init();
    UI_ClearRenderCounters();
    HOT_RELOAD_InitWatcher();
  }

  NET_Init();
  OBJ_Init();
}

GAME_Iterate :: ()
{
  // pre frame logic
  {
    new_timestamp := GetTime(.NOW);
    delta_timestamp := min(new_timestamp - G.frame_timestamp, 100); // clamp dt to 100ms (10fps)

    G.frame_number += 1;
    G.frame_timestamp = new_timestamp;
    G.tick_timestamp_accumulator += delta_timestamp;
    G.dt = delta_timestamp * 0.001;
    G.at = WrapFloat(0, 1000, G.at + G.dt);
  }

  if KEY_Pressed(.F11) then G.window_fullscreen = !G.window_fullscreen;

  if !G.headless
  {
    AUDIO_CloseFinishedStreams();
    GPU_ReadbackMousePickingTexture();
  }

  // pre frame calls
  if G.window_resized
  {
    GPU_ResizeFramebuffer();
    FONT_MaybeResizeAtlasTexture();
  }

  NET_IterateReceive();
  TICK_Iterate();
  NET_IterateTimeoutUsers();
  NET_IterateSend();

  if !G.headless && G.frame_number == 1
  {
    // print("@todo remove this temporary code\n");
    // for scale_part: 1 .. 8
    // {
    //   scale := 16 + scale_part * 4;
    //   for codepoint: 33 .. 126
    //   {
    //     // g0 := FONT_GetGlyph(xx codepoint, .Jacquard24,      xx scale);
    //     g1 := FONT_GetGlyph(xx codepoint, .PlayfairDisplay, xx scale);
    //   }
    // }
  }

  if !G.headless // @todo Some of this code should probably run in headless too.
  {
    UI_DeveloperKeyShortcuts();

    if G.dev.noclip
    {
      if KEY_Held(.MOUSE_LEFT)
      {
        rot_speed := 0.0004;
        G.camera_angles.z += rot_speed * G.mouse_delta.x;
        G.camera_angles.y += rot_speed * G.mouse_delta.y * (-2.0 / 3.0);
        G.camera_angles.z = WrapFloat(-0.5, 0.5, G.camera_angles.z);
        G.camera_angles.y = clamp(G.camera_angles.y, -0.2, 0.2);
      }

      move_dir: V3;
      if (KEY_Held(.W)) move_dir.x += 1;
      if (KEY_Held(.S)) move_dir.x -= 1;
      if (KEY_Held(.A)) move_dir.y += 1;
      if (KEY_Held(.D)) move_dir.y -= 1;
      if (KEY_Held(.SPACE)) move_dir.z += 1;
      if (KEY_Held(.LSHIFT) || KEY_Held(.RSHIFT)) move_dir.z -= 1;
      move_dir = normalize(move_dir);
      move_dir = Math.rotate(move_dir, RotationAroundAxis(-AxisV3(.Z)/* @todo why negation fixes things? */, G.camera_angles.z));
      G.camera_p += move_dir * 4.0 * G.dt;
    }
    else
    {
      G.camera_p.z = 12.5;
      G.camera_angles = V3.{0, -0.138, 0};

      // Mouse window edge camera scroll
      if !G.dev.show && OverlapsRange(MakeRange(G.window_dim), G.mouse)
      {
        window_margin := min(G.window_dim.x, G.window_dim.y) * 0.025;
        ratio := 1.0 / window_margin;

        dir: V2;
        dir.y += max(window_margin - G.mouse.x, 0) * ratio;
        dir.x += max(window_margin - G.mouse.y, 0) * ratio;
        dir.y -= max(G.mouse.x - (G.window_dim.x - window_margin), 0) * ratio;
        dir.x -= max(G.mouse.y - (G.window_dim.y - window_margin), 0) * ratio;

        easing :: (x: float) -> float {
          // return 1.0 - pow(1 - x, 4); // easeOutQuart
          return x*x*x;
        }
        dir.x = Smoothstep(abs(dir.x), max=0.9) * Sign(dir.x);
        dir.y = Smoothstep(abs(dir.y), max=0.9) * Sign(dir.y);

        dir_len := length(dir);
        if dir_len > 1.0 then dir /= dir_len;
        G.camera_p.xy += dir * G.dt * 16.0;
      }

      player := OBJ_Get(G.client.player_key, .NETWORK);
      if KEY_Held(.SPACE) ||
         G.frame_number < 10 // temp hack to center camera on player on startup
      {
        G.camera_p.xy = player.s.p.xy;
        G.camera_p.x -= 8.5;
      }
    }

    // Sun things
    {
      // Move sun
      sun_time := G.at*4.0;
      sun_time = 6.0;

      sun_x := Sin(0.5 + sun_time * 0.03);
      sun_y := Cos(0.5 + sun_time * 0.03);
      towards_sun_dir := normalize(V3.{sun_x, sun_y, 2.0});
      G.sun_dir = -towards_sun_dir;

      sun_dist := towards_sun_dir * 40.0;
      G.sun_camera_p = G.camera_p + sun_dist;
      OBJ_Get(G.obj.sun, .OFFLINE).s.p = G.sun_camera_p;

      transl := TranslationMatrix(-G.sun_camera_p);
      rot := RotationMatrix(RotationFromPair(G.sun_dir, AxisV3(.X)));

      dim := 20.0;
      projection := OrthographicMatrix(-dim, dim, -dim, dim, 30.0, 100.0);
      G.sun_camera_transform = projection * (rot * transl);
    }

    // calculate camera transform matrices
    {
      transl := TranslationMatrix(-G.camera_p);
      rot := RotationMatrixAroundAxis(AxisV3(.X), G.camera_angles.x);
      rot = RotationMatrixAroundAxis(AxisV3(.Z), G.camera_angles.z) * rot;
      rot = RotationMatrixAroundAxis(AxisV3(.Y), G.camera_angles.y) * rot;
      aspect := G.window_dim.x / G.window_dim.y;
      projection := PerspectiveMatrix(G.camera_fov_y, aspect, 0.25, 25.0);
      G.camera_transform = projection * (rot * transl);
    }

    // calculate mouse in screen space -> mouse in world space (at Z == 0)
    {
      view_mouse := V2.{
        (G.mouse.x / G.window_dim.x) *  2.0 - 1.0,
        (G.mouse.y / G.window_dim.y) * -2.0 + 1.0,
      };

      plane_origin: V3;
      plane_normal := V3.{0,0,1};

      aspect := G.window_dim.x / G.window_dim.y;
      tan := Tan(G.camera_fov_y * 0.5);
      aspect *= aspect;
      tan    *= tan;

      x := tan * view_mouse.x * aspect;
      y := tan * view_mouse.y;
      aim_dir := V3.{
        x * G.camera_transform.elem[0][0] + y * G.camera_transform.elem[0][1] + G.camera_transform.elem[0][2],
        x * G.camera_transform.elem[1][0] + y * G.camera_transform.elem[1][1] + G.camera_transform.elem[1][2],
        x * G.camera_transform.elem[2][0] + y * G.camera_transform.elem[2][1] + G.camera_transform.elem[2][2],
      };

      t_numerator := dot(plane_origin - G.camera_p, plane_normal);
      t_denominator := dot(aim_dir, plane_normal);

      G.world_mouse_valid = t_denominator < 0.0;
      if G.world_mouse_valid
      {
        t := t_numerator / t_denominator;
        world_off := aim_dir * t;
        world_mouse := G.camera_p + world_off;
        G.world_mouse = V3.{xy = world_mouse.xy, z = 0}; // @todo pass real Z?
      }
    }

    ANIMATION_AnimateObjects();
    AUDIO_PlayObjectSounds();

    // Process gameplay actions
    {
      move_buttons := KEY_Code.[.MOUSE_RIGHT, .Z, .X];
      move_pressed := KEY_Pressed(..move_buttons);
      move_held := KEY_Held(..move_buttons, held_for=250);

      if G.world_mouse_valid
      {
        hovered_obj := OBJ_Get(G.hover_object, .ALL);
        hovers_targetable_non_self := OBJ_HasAnyFlag(hovered_obj, .TARGETABLE) && G.hover_object != G.client.player_key;

        if move_pressed && hovers_targetable_non_self
        {
          Initialize(*G.action);
          G.action.type = .ATTACK;
          G.action.target_object = G.hover_object;
        }

        if (move_pressed && !hovers_targetable_non_self) ||
           (move_held && G.action.type == .PATHING)
        {
          Initialize(*G.action);
          G.action.type = .PATHING;
          G.action.world_p = G.world_mouse;
          if move_pressed then G.action.pressed_timestamp = GetTime();
        }
      }

      if !G.dev.noclip && KEY_Held(.W, .S, .A, .D)
      {
        dir: V3;
        if KEY_Held(.W) dir.x += 1;
        if KEY_Held(.S) dir.x -= 1;
        if KEY_Held(.A) dir.y += 1;
        if KEY_Held(.D) dir.y -= 1;

        Initialize(*G.action);
        G.action.type = .PATHING_DIRECTION;
        G.action.world_p = OBJ_Get(G.client.player_key, .NETWORK).s.p + dir;
      }
    }

    // Append action to client action queue
    {
      QueuePush(*G.client.action_queue, G.action);
    }

    marker := OBJ_Get(G.obj.pathing_marker, .OFFLINE);
    if !OBJ_IsNil(marker)
    {
      if G.action.type == .PATHING
      {
        marker.s.flags |= .DRAW_MODEL;
        marker.s.model = ModelKey("Flag");
        marker.s.p = G.action.world_p;

        marker.l.animated_p.x = marker.s.p.x;
        marker.l.animated_p.y = marker.s.p.y;

        if ElapsedTime(G.action.pressed_timestamp) == 0
          marker.l.animated_p.z = 0.5;

        G.obj.pathing_marker_set = true;
      }
      else
      {
        marker.s.p.z = -2.0;
        G.obj.pathing_marker_set = false;
      }
    }


    if KEY_Pressed(.M)
    {
      AUDIO_PlaySound(.HIT);
    }

    WORLD_DrawObjects();
    UI_Translate3DShapes();

    UI_StartFrame();
    UI_BuildDeveloperWindow();
    UI_FinishFrame();

    GPU_Iterate();

    // Post frame calls
    GPU_BATCH_PostFrame();
    UI_ClearRenderCounters();
    // UI_PostFrame();
    ASSET_PostFrame();

    HOT_RELOAD_Iterate();
  }

  // Post frame logic
  reset_temporary_storage();

  if !G.time_finished_first_iterate
  {
    G.time_finished_first_iterate = current_time_monotonic();
    ms :: to_milliseconds;

    init_total := G.time_init_finished - G.time_launch;
    init_sdl := G.time_init_sdl - G.time_launch;
    init_game := G.time_init_game - G.time_init_sdl;
    log("[TIMING] Init Total: %ms; Init SDL: %ms; Init Game: %ms", ms(init_total), ms(init_sdl), ms(init_game));

    first_iter := G.time_finished_first_iterate - G.time_init_finished;
    log("[TIMING] First iteration (excluding init): %ms", ms(first_iter));
  }

  if G.headless
  {
    // Since we don't wait for v-sync in headless mode
    // we will wait here to avoid using 100% of the CPU.
    // This should be solved in a better way (waiting on network msg or something)
    // once scheduling and multithreading is further along.
    SDL_Delay(16);
  }
}

GAME_ParseCommandLineArguments :: (args: [] string)
{
  parse_target: *float;
  for args
  {
    if it_index == 0  continue; // skip arg with executable path
    arg := it;

    if parse_target
    {
      value, success := string_to_int(arg);
      if !success
      {
        log_error("Failed to parse % as an integer number.\n", arg);
        exit(1);
      }
      parse_target.* = cast(float) value;
      parse_target = null;
    }
    else
    {
      if arg ==
      {
        case "-headless";   G.headless = true;
        case "-w";          parse_target = *G.init_window_dim.x;
        case "-h";          parse_target = *G.init_window_dim.y;
        case "-px";         parse_target = *G.init_window_p.x;
        case "-py";         parse_target = *G.init_window_p.y;
        case "-fullscreen"; G.window_fullscreen = true;
        case "-borderless"; G.window_borderless = true;
        case "-top";        G.window_on_top = true;
        case "-autolayout"; G.window_autolayout = true;
        case "-server";     G.net.is_server = true;
        case "-exit-on-dc"; G.server_exit_on_disconnect = true;

        case;
        log_error("Invalid command line argument %s.\n", arg);
        exit(1);
      }
    }

    print("arg%: %\n", it_index, it);
  }
}

GAME_AutoLayoutApply :: (code: ..Code) #expand
{
  // @impl
  log_error("GAME_AutoLayoutApply called but not implemented");
}

// Includes and stuff
#import "Basic";
#import "File";
#import "Thread";
String :: #import "String";

#load "../shared/base_utils.jai";
#load "../shared/base_string.jai";
#load "../shared/base_array.jai";
#load "../shared/base_memory.jai";
#load "../shared/base_hash.jai";
#load "../shared/base_math.jai";
#load "../shared/base_color.jai";
#load "../shared/pie_file_format.jai";
#load "../shared/game_render_world_shared.jai";

#load "game_ui_text.jai";
#load "game_ui_core.jai";
#load "game_ui_user.jai";
#load "game_time.jai";
#load "game_key.jai";
#load "game_action.jai";
#load "game_animation.jai";
#load "game_asset.jai";
#load "game_asset_loading.jai";
#load "game_asset_hot_reload.jai";
#load "game_object.jai";
#load "game_object_collider.jai";
#load "game_network.jai";
#load "game_client.jai";
#load "game_server.jai";
#load "game_tick.jai";
#load "game_audio.jai";
#load "game_gpu.jai";
#load "game_gpu_batch.jai";
#load "game_render_world.jai";
#load "game_render_ui.jai";
#load "game_font.jai";
#load "game_util.jai";
#load "game_tests.jai";
#load "game_pie_loader.jai";
