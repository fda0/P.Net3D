#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"

#define THIS_DIR "../src/meta/"

static S8 Os_LoadFile(const char *file_path)
{
    U64 size = 0;
    void *data = SDL_LoadFile(file_path, &size);
    S8 result =
    {
        (U8 *)data,
        size
    };
    return result;
}

int main()
{
    S8 teapot = Os_LoadFile("res/models/teapot.obj");
    (void)teapot;

    printf("hello sailors\n");
    return 0;
}
