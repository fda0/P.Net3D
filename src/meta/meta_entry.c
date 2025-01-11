#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"

#define THIS_DIR "../src/meta/"

typedef struct
{
    U32 log_filter;
} Meta_State;
static Meta_State M;

typedef enum
{
    MetaLog_Idk   = (1 << 0),
    MetaLog_Error = (1 << 1),
} Log_Flags;

// logging enable/disable
#if 1
    #define META_LOG(FLAGS, ...) do{ if(((FLAGS) & M.log_filter) == (FLAGS)){ SDL_Log("[META] " __VA_ARGS__); }}while(0)
#else
    #define META_LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ printf("[META] " __VA_ARGS__); }}while(0)
#endif

static S8 M_LoadFile(const char *file_path, bool exit_on_err)
{
    U64 size = 0;
    void *data = SDL_LoadFile(file_path, &size);
    if (exit_on_err && !data)
    {
        META_LOG(MetaLog_Error, "Failed to load file %s, exiting", file_path);
        exit(0);
    }

    S8 result =
    {
        (U8 *)data,
        size
    };
    return result;
}

int main()
{
    // init
    M.log_filter = ~0u;

    // work
    {
        S8 teapot = M_LoadFile("../res/models/teapot.obj", true);
        (void)teapot;
    }

    // exit
    META_LOG(MetaLog_Idk, "Success!");
    return 0;
}
