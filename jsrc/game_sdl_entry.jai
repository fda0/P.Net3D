main :: ()
{
  PLATFORM_Init();
  PLATFORM_Iterate();
  PLATFORM_Deinit();
}

PLATFORM_Init :: ()
{
  G.dpi_scaling = 1;

  ok := SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  if ok
  {
    GAME_ParseCommandLineArguments(get_command_line_arguments());

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
  }

  if ok
    ok = SDLNet_Init();

  if ok && !G.headless
  {
    G.gpu.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, debug_mode = true, name = null);
    ok = !!G.gpu.device;
  }

  if ok && !G.headless
  {
    G.window_dim = G.init_window_dim;
    window_flags: SDL_WindowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    if G.window_on_top      window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    if G.window_borderless  window_flags |= SDL_WINDOW_BORDERLESS;

    G.window = SDL_CreateWindow("Treasure", xx G.window_dim.x, xx G.window_dim.y, window_flags);
    ok = !!G.window;
  }

  if ok && !G.headless
  {
    ok = SDL_ClaimWindowForGPUDevice(G.gpu.device, G.window);
  }

  if ok && !G.headless && (G.init_window_p.x > 0 || G.init_window_p.y)
  {
    x := ifx G.init_window_p.x > 0 then xx G.init_window_p.x else cast(s32) SDL_WINDOWPOS_UNDEFINED();
    y := ifx G.init_window_p.y > 0 then xx G.init_window_p.y else cast(s32) SDL_WINDOWPOS_UNDEFINED();
    SDL_SetWindowPosition(G.window, x, y);
  }

  GAME_Init();

  if ok && !G.headless
    SDL_ShowWindow(G.window);

  if !ok
  {
    log_error("Failed to initialize SDL window\n");
    exit(1);
  }
}

PLATFORM_Iterate :: ()
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

    GAME_Iterate();

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

// @todo improve generator so you don't need to define these here:
SDL_BUTTON_LMASK : SDL_MouseButtonFlags : (1 << 0);
SDL_BUTTON_RMASK : SDL_MouseButtonFlags : (1 << 2);
// @todo how to overwrite SDL_ASSERT_LEVEL :: 2; in sdl3 module?

using,map(ImportMapForSDL) SDL :: #import,file "libs/jai-sdl3/module.jai";
ImportMapForSDL :: (names: [] string)
{
  S :: #import,file "base_string.jai";
  B :: #import "Basic";

  for *names
  {
    name := it.*;
    new_name: string;

    if S.STR_StartsWith(name, "NET_")
      new_name = B.tprint("SDLNet_%", S.STR_Skip(name, 4));
    else if !S.STR_StartsWith(name, "SDL")
      new_name = B.tprint("SDL%", name);

    if new_name.count
    {
      // B.print("SDL RENAME % -> %\n", name, new_name);
      it.* = new_name;
    }
  }
}
