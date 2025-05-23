main :: ()
{
  ok := SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  renderer: *SDL_Renderer;
  if ok {
    ok = SDL_CreateWindowAndRenderer("Treasure", 1920, 1080, SDL_WINDOW_OPENGL, *G.window, *renderer);
  }

  if !ok {
    print("Failed to initialize SDL things\n");
    exit(1);
  }

  while !G.in_shutdown
  {
    // Poll input
    {
      ev: SDL_Event;
      while(SDL_PollEvent(*ev))
      {
        if ev.type ==
        {
          case xx SDL_EventType.QUIT;
          G.in_shutdown = true;

          case xx SDL_EventType.KEY_DOWN;
          if ev.key.key == SDLK_ESCAPE  G.in_shutdown = true;

          case;
        }
      }

      mouse_keys := SDL_GetMouseState(*G.mouse.x, *G.mouse.y);
      KEY_Update(KEY.MouseLeft, mouse_keys & SDL_BUTTON_LMASK);
      KEY_Update(KEY.MouseRight, mouse_keys & SDL_BUTTON_RMASK);
    }

    // Debug exit cleanup - to check for resource leaks
    {
      // SDLNet_Quit();
      // call something that checks leaks in SDL
    }
  }

}

#load "game.jai";
#import "Basic";
#import "Math";
#import,file "libs/jai-sdl3/module.jai"; // @todo how to overwrite SDL_ASSERT_LEVEL :: 2; in sdl3 module?
