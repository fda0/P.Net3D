GAME_State :: struct {
  // core game state
  in_shutdown: bool;

  // component states
  gpu: GPU_State;

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

  // user input
  mouse: V2;
  mouse_delta: V2;
  mouse_scroll: V2;
  world_mouse_valid: bool;
  world_mouse: V2;
  keys: [KEY_Code.COUNT] Key;
}
G: GAME_State;

GAME_Init :: ()
{
  if !G.headless
  {
    GPU_Init();
  }
}

GAME_Iter :: ()
{

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
        // case "-server";     G.net.is_server = true;
        // case "-exit-;on-dc" G.server.exit_on_disconnect = true;

        case;
        log_error("Invalid command line argument %s.\n", arg);
        exit(1);
      }
    }

    print("arg%: %\n", it_index, it);
  }
}

// Includes and stuff
#load "game_base.jai";
#load "game_key.jai";
#load "game_animation.jai";
#load "game_asset.jai";
#load "game_gpu.jai";
#load "game_gpu_batch.jai";
#load "game_render_world.jai";
#load "game_render_ui.jai";
#load "game_precompiled_shaders_tmp.jai";

