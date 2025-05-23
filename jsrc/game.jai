V2 :: Vector2;

GAME_State :: struct {
  // SDL, window stuff
  window: *SDL_Window;
  dpi_scaling: float;
  in_shutdown: bool;

  mouse: V2;
}
G: GAME_State;
