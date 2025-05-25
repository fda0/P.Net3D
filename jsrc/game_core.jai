V2 :: Vector2;

GAME_State :: struct {
  // core game state
  in_shutdown: bool;

  // SDL, window stuff
  window: *SDL_Window;
  dpi_scaling: float;
  fullscreen: bool;
  headless: bool; // no window mode
  init_window_p: V2 = .{-1, -1};
  init_window_dim: V2 = .{-1, -1};
  window_dim: V2;
  window_resized: bool; // window resized on this frame
  window_on_top: bool;
  window_borderless: bool;

  // user input
  mouse: V2;
  mouse_delta: V2;
  mouse_scroll: V2;
  world_mouse_valid: bool;
  world_mouse: V2;
  keys: [KEY_Code.COUNT] Key;
}
G: GAME_State;
