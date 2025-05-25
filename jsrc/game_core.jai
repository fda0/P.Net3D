V2 :: Vector2;

GAME_State :: struct {
  // core game state
  in_shutdown: bool;

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


GAME_ParseCommandLineArguments :: (args: [] string)
{
  for args
  {
    if it_index == 0  continue; // skip arg with executable path
    arg := it;

    if arg ==
    {
      case "-headless";   G.headless = true;
      // case "-w"; etc
      case "-fullscreen"; G.window_fullscreen = true;
      case "-borderless"; G.window_borderless = true;
      case "-top";        G.window_on_top = true;
      case "-autolayout"; G.window_autolayout = true;
      // case "-server";     G.net.is_server = true;
      // case "-exit-;on-dc" G.server.exit_on_disconnect = true;
    }

    print("arg%: %\n", it_index, it);
  }
}
