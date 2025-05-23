main :: () {
  ok := SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  renderer: *SDL_Renderer;
  if ok {
    ok = SDL_CreateWindowAndRenderer("Treasure", 1920, 1080, SDL_WINDOW_OPENGL, *G.window, *renderer);
  }

  if !ok {
    print("Failed to initialize SDL things\n");
    exit(1);
  }

  while !G.in_shutdown {

    ev: SDL_Event;
    while(SDL_PollEvent(*ev)) {

      if ev.type == {

        case xx SDL_EventType.QUIT;
        G.in_shutdown = true;

        case xx SDL_EventType.KEY_DOWN;
        if ev.key.key == SDLK_ESCAPE  G.in_shutdown = true;

        case;
      }
    }
  }

  // Exit cleanup (not really needed - mostly done to check for leaks)
  {
    // SDLNet_Quit();
  }
}

#load "game.jai";
#import "Basic";
#import,file "libs/jai-sdl3/module.jai"; // @todo how to overwrite SDL_ASSERT_LEVEL :: 2; in sdl3 module?
