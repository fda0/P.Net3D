GameState :: struct {
  // SDL, window stuff
  window: *SDL_Window;
  dpi_scaling: float;
  in_shutdown: bool;
}
G: GameState;
