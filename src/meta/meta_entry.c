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
        M_LOG(M_LogErr, "Failed to load file %s", file_path);
        exit(1);
    }

    S8 result =
    {
        (U8 *)data,
        size
    };
    return result;
}

static void M_SaveFile(const char *file_path, S8 data)
{
    bool success = SDL_SaveFile(file_path, data.str, data.size);
    if (!success)
    {
        M_LOG(M_LogErr, "Failed to save to file %s", file_path);
        exit(1);
    }
}

#include "meta_parse_obj.c"

int main()
{
    // init
    M.log_filter = ~(U32)(M_LogObjDebug);
    M.tmp = Arena_MakeInside(arena_memory_buffer, sizeof(arena_memory_buffer));

    // work
    {
        U64 tmp_used = M.tmp->used; // save tmp arena used

        Printer pr_out = Pr_Alloc(M.tmp, Megabyte(1));
        M_ParseObj("../res/models/teapot.obj", &pr_out);
        M_SaveFile("../src/gen/gen_models.hx", Pr_S8(&pr_out));

        Arena_Reset(M.tmp, tmp_used); // restore tmp arena used
    }


    // exit
    M_LOG(M_LogIdk, "Success");
    return 0;
}
