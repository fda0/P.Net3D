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
#include <SDL3_image/SDL_image.h>
#include <SDL3_net/SDL_net.h>

#define WINDOW_HEIGHT 640
#define WINDOW_WIDTH 854

#include "base_types.h"
#include "base_math.h"
#include "base_string.h"
#include "de_constants.h"
#include "de_math.h"
#include "de_vertices.h"
#include "de_util.h"
#include "de_object.h"
#include "de_tick.h"
#include "de_network.h"
#include "de_client.h"
#include "de_server.h"
#include "de_render_vertex.h"
#include "de_gpu.h"
#include "de_main.h"

static AppState APP;
#include "de_tests.c"
#include "de_sprite.c"
#include "de_object.c"
#include "de_client.c"
#include "de_server.c"
#include "de_network.c"
#include "de_tick.c"
#include "de_main.c"

#include "gen_shader_model.vert.hx"
#include "gen_shader_model.frag.hx"
#include "gen_shader_wall.vert.hx"
#include "gen_shader_wall.frag.hx"
#include "gen_models.hx"
#include "de_gpu.c"

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *app = (AppState*)appstate;

    // input
    {
        app->mouse_keys = SDL_GetMouseState(&app->mouse.x, &app->mouse.y);

        // keyboard state
        {
            int numkeys = 0;
            const bool *key_state_arr = SDL_GetKeyboardState(&numkeys);
            int to_copy = Min(numkeys, ArrayCount(app->keyboard));
            memcpy(app->keyboard, key_state_arr, to_copy);
        }
    }

    //SDL_SetRenderDrawColor(app->renderer, 64, 64, 64, 255);
    //SDL_RenderClear(app->renderer);
    Game_Iterate(app);
    //SDL_RenderPresent(app->renderer);

    Gpu_Iterate();

    // render cleanup
    {
        APP.rdr.instance_count = 0;
        APP.rdr.wall_vert_count = 0;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *app = (AppState *)appstate;

    switch (event->type)
    {
        case SDL_EVENT_QUIT:
        {
            return SDL_APP_SUCCESS;
        } break;

        case SDL_EVENT_WINDOW_RESIZED:
        {
            app->window_width = event->window.data1;
            app->window_height = event->window.data2;
        } break;

        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_KEY_DOWN:
        {
            if (event->key.key == SDLK_ESCAPE)
            {
                return SDL_APP_SUCCESS; // useful in development
            }

            if (event->type == SDL_EVENT_KEY_DOWN &&
                event->key.key == SDLK_P)
            {
                app->debug.unpause_one_tick = true;
            }
        } break;

        default:
        {
        } break;
    }

    return SDL_APP_CONTINUE;
}

static void Game_ParseCmd(AppState *app, int argc, char **argv)
{
    for (int i = 1; i < argc; i += 1)
    {
        const char *arg = argv[i];
        if (0 == strcmp(arg, "-server"))
        {
            app->net.is_server = true;
        }
        else if (0 == strcmp(arg, "-top"))
        {
            app->window_on_top = true;
        }
        else if (0 == strcmp(arg, "-b"))
        {
            app->window_borderless = true;
        }
        else if (0 == strcmp(arg, "-w") ||
                 0 == strcmp(arg, "-h") ||
                 0 == strcmp(arg, "-px") ||
                 0 == strcmp(arg, "-py"))
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
                    if      (0 == strcmp(arg, "-w"))  app->init_window_width = number;
                    else if (0 == strcmp(arg, "-h"))  app->init_window_height = number;
                    else if (0 == strcmp(arg, "-px")) app->init_window_px = number;
                    else if (0 == strcmp(arg, "-py")) app->init_window_py = number;
                }
            }

            if (!found_number)
            {
                LOG(LogFlags_Idk, "%s needs to be followed by positive number", arg);
            }
        }
        else if (0 == strcmp(arg, "-autolayout"))
        {
            app->window_autolayout = true;
        }
        else
        {
            LOG(LogFlags_Idk, "Unhandled argument: %s", arg);
        }
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    //*appstate = SDL_calloc(1, sizeof(AppState));
    *appstate = &APP;
    {
        void *aligned = AlignPointerUp(appstate, _Alignof(AppState));
        Assert(aligned == appstate);
    }

    AppState *app = (AppState *)*appstate;
    if (!app)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to allocate appstate", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }
    app->log_filter = ~(U32)LogFlags_NetAll;
    app->init_window_width = WINDOW_WIDTH;
    app->init_window_height = WINDOW_HEIGHT;

    Game_ParseCmd(app, argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to initialize SDL3.", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (!SDLNet_Init())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to initialize SDL3 Net.", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    window_flags |= (app->window_on_top ? SDL_WINDOW_ALWAYS_ON_TOP : 0);
    window_flags |= (app->window_borderless ? SDL_WINDOW_BORDERLESS : 0);

    app->window_width = app->init_window_width;
    app->window_height = app->init_window_height;

#if 1
    APP.gpu.device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,
                            /* debug mode */ true,
                            /* const char * name*/ 0);

    if (!APP.gpu.device)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPUCreateDevice failed", SDL_GetError(), 0);
        return SDL_APP_FAILURE;
    }

    APP.window = SDL_CreateWindow("P. Game", app->window_width, app->window_height, window_flags);
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

#else
    if (!SDL_CreateWindowAndRenderer("P Game",
                                     app->window_width, app->window_height,
                                     window_flags,
                                     &app->window, &app->renderer))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to create window/renderer", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }
#endif

    if (app->init_window_px || app->init_window_py)
    {
        int x = (app->init_window_px ? app->init_window_px : SDL_WINDOWPOS_UNDEFINED);
        int y = (app->init_window_py ? app->init_window_py : SDL_WINDOWPOS_UNDEFINED);
        SDL_SetWindowPosition(app->window, x, y);
    }

    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(app->window));
    SDL_Log("Screen bpp: %d\n", SDL_BITSPERPIXEL(mode->format));

    Gpu_Init();

    SDL_ShowWindow(app->window);

#if 0
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
#endif
    Game_Init(app);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    (void)appstate;

    SDLNet_Quit();

    const char* error = SDL_GetError();
    if (error[0])
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s\n", error);
    }
}
