#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>
#include "stdio.h"
#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"
#include "base_math.h"
#include "base_printer.h"
#include "de_render_vertex.h"

#include "meta_entry.h"
#include "meta_print_parse.c"
#include "meta_obj.c"

int main()
{
    // init
    M.log_filter = ~(U32)(M_LogObjDebug);
    M.tmp = Arena_MakeInside(arena_memory_buffer, sizeof(arena_memory_buffer));

    // work
    {
        U64 tmp_used = M.tmp->used; // save tmp arena used

        Printer pr_out = Pr_Alloc(M.tmp, Megabyte(1));
        M_ParseObj("../res/models/teapot.obj", &pr_out, (M_ModelSpec){.scale = 10.f, .rot_x = 0.25f});
        M_SaveFile("../src/gen/gen_models.hx", Pr_S8(&pr_out));

        Arena_Reset(M.tmp, tmp_used); // restore tmp arena used
    }


    // exit
    M_LOG(M_LogIdk, "Success");
    return 0;
}
