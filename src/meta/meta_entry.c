#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>
#include "stdio.h"
#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"
#include "base_math.h"
#include "base_printer.h"
#include "de_render_vertex.h"

static U8 arena_memory_buffer[Megabyte(32)]; // 32MB
typedef struct
{
    U32 log_filter;
    Arena *tmp;
} Meta_State;
static Meta_State M;

enum M_LogType
{
    M_LogIdk = (1 << 0),
    M_LogErr = (1 << 1),
    M_LogObjDebug = (1 << 2),
};

// logging enable/disable
#if 1
    #define M_LOG(FLAGS, ...) do{ if(((FLAGS) & M.log_filter) == (FLAGS)){ SDL_Log("[META] " __VA_ARGS__); }}while(0)
#else
    #define M_LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ printf("[META] " __VA_ARGS__); }}while(0)
#endif

static S8 M_LoadFile(const char *file_path, bool exit_on_err)
{
    U64 size = 0;
    void *data = SDL_LoadFile(file_path, &size);
    if (exit_on_err && !data)
    {
        M_LOG(M_LogErr, "Failed to load file %s, exiting", file_path);
        exit(1);
    }

    S8 result =
    {
        (U8 *)data,
        size
    };
    return result;
}

#include "meta_parse_obj.c"

int main()
{
    // init
    M.log_filter = ~(U32)(M_LogObjDebug);
    M.tmp = Arena_MakeInside(arena_memory_buffer, sizeof(arena_memory_buffer));

    // work
    M_ParseObj("../res/models/teapot.obj");

    // exit
    M_LOG(M_LogIdk, "Success");
    return 0;
}
