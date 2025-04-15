#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL_stdinc.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <float.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_net/SDL_net.h>
#include <SDL3_image/SDL_image.h>

#define WINDOW_HEIGHT 640
#define WINDOW_WIDTH 854

#include "base_types.h"
#include "base_math.h"
#include "base_string.h"
#include "base_arena.h"
#include "base_hash.h"
#include "base_printer.h"
#include "game_constants.h"
#include "game_collision_vertices.h"
#include "game_util.h"
#include "game_asset_definitions.h"
#include "game_asset.h"
#include "game_render.h"
#include "game_object.h"
#include "game_tick.h"
#include "game_network.h"
#include "game_client.h"
#include "game_server.h"
#include "game_animation.h"
#include "game_gpu.h"
#include "game_font_atlas.h"
#include "game_core.h"

#include "gen_shader_rigid.vert.h"
#include "gen_shader_rigid.frag.h"
#include "gen_shader_skinned.vert.h"
#include "gen_shader_skinned.frag.h"
#include "gen_shader_mesh.vert.h"
#include "gen_shader_mesh.frag.h"
#include "gen_shader_ui.vert.h"
#include "gen_shader_ui.frag.h"
#include "gen_models.h"
#include "gen_models_gltf.h"
#include "gen_animations.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

static AppState APP;
#include "game_util.c"
#include "game_asset.c"
#include "game_key.c"
#include "game_object.c"
#include "game_client.c"
#include "game_server.c"
#include "game_network.c"
#include "game_tick.c"
#include "game_animation.c"
#include "game_render.c"
#include "game_gpu.c"
#include "game_font_atlas.c"
#include "game_ui.c"
#include "game_core.c"

SDL_AppResult SDL_AppIterate(void *appstate)
{
  (void)appstate;

  // Poll input
  {
    // Mouse
    SDL_MouseButtonFlags mouse_keys = SDL_GetMouseState(&APP.mouse.x, &APP.mouse.y);

    KEY_Update(KEY_MouseLeft, mouse_keys & SDL_BUTTON_LMASK);
    KEY_Update(KEY_MouseRight, mouse_keys & SDL_BUTTON_RMASK);

    // Keyboard
    I32 num_keys = 0;
    const bool *key_states = SDL_GetKeyboardState(&num_keys);
    ForI32(i, num_keys)
    {
      KEY_Update(i, key_states[i]);
    }
  }

  Game_Iterate();

  //
  // Post frame
  //
  if (!APP.timestamp_post_first_frame)
  {
    APP.timestamp_post_first_frame = SDL_GetTicks();
    U64 startup_delta = APP.timestamp_post_first_frame - APP.timestamp_app_launch;
    LOG(Log_Perf, "Launch to first frame took: %llums", startup_delta);
  }

  // Post frame input cleanup
  {
    APP.mouse_scroll = (V2){};
    APP.mouse_delta = (V2){};
  }

  APP.window_resized = false;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
  (void)appstate;

  switch (event->type)
  {
    case SDL_EVENT_QUIT:
    {
      return SDL_APP_SUCCESS;
    } break;

    case SDL_EVENT_WINDOW_RESIZED:
    {
      APP.window_width = event->window.data1;
      APP.window_height = event->window.data2;
      APP.window_dim = (V2){APP.window_width, APP.window_height};
      APP.window_resized = true;
    } break;

    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_KEY_DOWN:
    {
      if (event->key.key == SDLK_ESCAPE)
      {
        return SDL_APP_SUCCESS; // useful in development
      }

      if (event->type == SDL_EVENT_KEY_DOWN)
      {
        if (event->key.key == SDLK_P)
          APP.debug.unpause_one_tick = true;

        if (event->key.key == SDLK_F11 && !APP.headless)
        {
          APP.fullscreen = !APP.fullscreen;
          SDL_SetWindowFullscreen(APP.window, APP.fullscreen);
        }
      }
    } break;

    case SDL_EVENT_MOUSE_MOTION:
    {
      SDL_MouseMotionEvent motion = event->motion;
      V2 delta = (V2){motion.xrel, motion.yrel};
      APP.mouse_delta = V2_Add(APP.mouse_delta, delta);
    } break;

    case SDL_EVENT_MOUSE_WHEEL:
    {
      SDL_MouseWheelEvent wheel = event->wheel;
      V2 delta = (V2){wheel.x, wheel.y};
      APP.mouse_scroll = V2_Add(APP.mouse_scroll, delta);
    }
  }

  return SDL_APP_CONTINUE;
}

static void Game_ParseCmd(int argc, char **argv)
{
  for (int i = 1; i < argc; i += 1)
  {
    S8 arg = S8_ScanCstr(argv[i]);
    if (S8_Match(arg, S8Lit("-headless"), 0))
    {
      APP.headless = true;
    }
    else if (S8_Match(arg, S8Lit("-server"), 0))
    {
      APP.net.is_server = true;
    }
    else if (S8_Match(arg, S8Lit("-top"), 0))
    {
      APP.window_on_top = true;
    }
    else if (S8_Match(arg, S8Lit("-b"), 0))
    {
      APP.window_borderless = true;
    }
    else if (S8_Match(arg, S8Lit("-w"), 0) ||
             S8_Match(arg, S8Lit("-h"), 0) ||
             S8_Match(arg, S8Lit("-px"), 0) ||
             S8_Match(arg, S8Lit("-py"), 0))
    {
      bool found_number = false;
      if (i + 1 < argc)
      {
        i += 1;
        const char *next_arg = argv[i];

        int number = SDL_strtoul(next_arg, 0, 0);
        if (number > 0)
        {
          found_number = true;
          if      (S8_Match(arg, S8Lit("-w"), 0))  APP.init_window_width = number;
          else if (S8_Match(arg, S8Lit("-h"), 0))  APP.init_window_height = number;
          else if (S8_Match(arg, S8Lit("-px"), 0)) APP.init_window_px = number;
          else if (S8_Match(arg, S8Lit("-py"), 0)) APP.init_window_py = number;
        }
      }

      if (!found_number)
      {
        LOG(Log_Idk, "%.*s needs to be followed by positive number", S8Print(arg));
      }
    }
    else if (S8_Match(arg, S8Lit("-autolayout"), 0))
    {
      APP.window_autolayout = true;
    }
    else
    {
      LOG(Log_Idk, "Unhandled argument: %.*s", S8Print(arg));
    }
  }
}

static Arena *Arena_Malloc(U64 size)
{
  return Arena_MakeInside(malloc(size), size);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
  (void)appstate;
  APP.timestamp_app_launch = SDL_GetTicks();

  APP.tmp = Arena_Malloc(Megabyte(16));
  APP.a_frame = Arena_Malloc(Megabyte(1));
  APP.log_filter = ~(U32)Log_NetAll;
  APP.init_window_width = WINDOW_WIDTH;
  APP.init_window_height = WINDOW_HEIGHT;
  APP.dpi_scaling = 1.f;

  Game_ParseCmd(argc, argv);

  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to initialize SDL3.", SDL_GetError(), NULL);
    return SDL_APP_FAILURE;
  }

  if (!SDLNet_Init())
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to initialize SDL Net.", SDL_GetError(), NULL);
    return SDL_APP_FAILURE;
  }

  if (!TTF_Init())
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to initialize SDL TTF.", SDL_GetError(), NULL);
    return SDL_APP_FAILURE;
  }

  if (!APP.headless)
  {
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    window_flags |= (APP.window_on_top ? SDL_WINDOW_ALWAYS_ON_TOP : 0);
    window_flags |= (APP.window_borderless ? SDL_WINDOW_BORDERLESS : 0);

    APP.window_width = APP.init_window_width;
    APP.window_height = APP.init_window_height;
    APP.window_dim = (V2){APP.window_width, APP.window_height};

    APP.gpu.device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,
                          /* debug mode */ true,
                          /* const char * name*/ 0);

    if (!APP.gpu.device)
    {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPUCreateDevice failed", SDL_GetError(), 0);
      return SDL_APP_FAILURE;
    }

    APP.window = SDL_CreateWindow("P. Game", APP.window_width, APP.window_height, window_flags);
    if (!APP.window)
    {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CreateWindow failed", SDL_GetError(), 0);
      return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(APP.gpu.device, APP.window))
    {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPUClaimWindow failed", SDL_GetError(), 0);
      return SDL_APP_FAILURE;
    }

    if (APP.init_window_px || APP.init_window_py)
    {
      int x = (APP.init_window_px ? APP.init_window_px : SDL_WINDOWPOS_UNDEFINED);
      int y = (APP.init_window_py ? APP.init_window_py : SDL_WINDOWPOS_UNDEFINED);
      SDL_SetWindowPosition(APP.window, x, y);
    }

    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(APP.window));
    SDL_Log("Screen bpp: %d\n", SDL_BITSPERPIXEL(mode->format));
  }

  if (APP.headless)
    SDL_Log("Starting in headless mode");

  Game_Init();

  if (!APP.headless)
    SDL_ShowWindow(APP.window);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
  (void)result;
  (void)appstate;

  SDLNet_Quit();
  AST_Deinit();
  GPU_Deinit();

  SDL_ReleaseWindowFromGPUDevice(APP.gpu.device, APP.window);
  SDL_DestroyWindow(APP.window);
  SDL_DestroyGPUDevice(APP.gpu.device);

  const char* error = SDL_GetError();
  if (error[0])
  {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s\n", error);
  }
}
