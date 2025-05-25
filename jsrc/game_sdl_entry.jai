main :: ()
{
  PLATFORM_Init();
  PLATFORM_Iter();
  PLATFORM_Deinit();
}

PLATFORM_Init :: ()
{
  ok := SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  // Query display resolution and calculate default window dimensions
  if G.init_window_dim.x < 0 || G.init_window_dim.y < 0
  {
    // @todo use:
    //       SDL_DisplayID SDL_GetPrimaryDisplay(void);
    //       bool SDL_GetDisplayUsableBounds(SDL_DisplayID displayID, SDL_Rect *rect);
    if G.init_window_dim.x < 0
      G.init_window_dim.x = 1920;

    if G.init_window_dim.y < 0
      G.init_window_dim.y = 1080;
  }

  renderer: *SDL_Renderer;
  if ok {
    ok = SDL_CreateWindowAndRenderer("Treasure", xx G.init_window_dim.x, xx G.init_window_dim.y, SDL_WINDOW_OPENGL, *G.window, *renderer);
  }

  if !ok {
    print("Failed to initialize SDL things\n");
    exit(1);
  }
}

PLATFORM_Iter :: ()
{
  while !G.in_shutdown
  {
    // Poll input
    {
      ev: SDL_Event;
      while SDL_PollEvent(*ev)
      {
        if ev.type ==
        {
          case xx SDL_EventType.QUIT;
          G.in_shutdown = true;

          case xx SDL_EventType.WINDOW_RESIZED;
          window := ev.window;
          G.window_dim = V2.{xx window.data1, xx window.data2};
          G.window_resized = true;

          case xx SDL_EventType.KEY_DOWN;
          if ev.key.key == SDLK_ESCAPE  G.in_shutdown = true;

          case xx SDL_EventType.MOUSE_MOTION;
          motion := ev.motion;
          delta := V2.{motion.xrel, motion.yrel};
          G.mouse_delta += delta;

          case xx SDL_EventType.MOUSE_WHEEL;
          wheel := ev.wheel;
          delta := V2.{wheel.x, wheel.y};
          G.mouse_scroll += delta;
        }
      }

      mouse_keys := SDL_GetMouseState(*G.mouse.x, *G.mouse.y);
      KEY_Update(.MouseLeft, !!(mouse_keys & SDL_BUTTON_LMASK));
      KEY_Update(.MouseRight, !!(mouse_keys & SDL_BUTTON_RMASK));

      num_keys: s32;
      key_states := SDL_GetKeyboardState(*num_keys);
      for 0..num_keys-1
        KEY_Update(xx it, key_states[it]);
    }

    // GAME_Iterate();

    // Poll input cleanup
    {
      G.mouse_scroll = V2.{};
      G.mouse_delta = V2.{};
      G.window_resized = false;
    }
  }
}

PLATFORM_Deinit :: ()
{
  // Debug exit cleanup - to check for resource leaks
  // SDLNet_Quit();
  // call something that checks leaks in SDL
}

#load "game_core.jai";
#load "game_key.jai";
#import "Basic";
#import "Math";


//
// SDL THINGS
//
/**
* A bitmask of pressed mouse buttons, as reported by SDL_GetMouseState, etc.
*
* - Button 1: Left mouse button
* - Button 2: Middle mouse button
* - Button 3: Right mouse button
* - Button 4: Side mouse button 1
* - Button 5: Side mouse button 2
*
* \since This datatype is available since SDL 3.2.0.
*
* \sa SDL_GetMouseState
* \sa SDL_GetGlobalMouseState
* \sa SDL_GetRelativeMouseState
SDL_MouseButtonFlags :: Uint32;
*/
SDL_BUTTON_LMASK : SDL_MouseButtonFlags : (1 << 0);
SDL_BUTTON_RMASK : SDL_MouseButtonFlags : (1 << 2);
#import,file "libs/jai-sdl3/module.jai"; // @todo how to overwrite SDL_ASSERT_LEVEL :: 2; in sdl3 module?
