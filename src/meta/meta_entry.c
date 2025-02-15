#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>
#include "stdio.h"
#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"
#include "base_math.h"
#include "base_printer.h"
#include "game_render.h"

#include "meta_obj.h"
#include "meta_entry.h"
#include "meta_print_parse.c"
#include "meta_obj.c"

#define CGLTF_IMPLEMENTATION
#pragma warning(push, 2)
#include "cgltf.h"
#pragma warning(pop)

#include "meta_gltf_loader.c"

static U8 arena_memory_buffer[Megabyte(64)];

int main()
{
  // init
  M.log_filter = ~(U32)(M_LogObjDebug | M_LogGltfDebug);
  //M.log_filter = ~(U32)(0);
  M.tmp = Arena_MakeInside(arena_memory_buffer, sizeof(arena_memory_buffer));

  // load .obj models
  {
    ArenaScope tmp_scope = Arena_PushScope(M.tmp);

    Printer pr_out = Pr_Alloc(M.tmp, Megabyte(1));
    M_ParseObj("../res/models/teapot.obj", &pr_out, (M_ModelSpec){.scale = 10.f, .rot_x = 0.25f});
    M_ParseObj("../res/models/flag.obj", &pr_out, (M_ModelSpec){.scale = 0.1f, .rot_x = 0.25f, .rot_z = 0.25f});
    M_SaveFile("../src/gen/gen_models.h", Pr_S8(&pr_out));

    Arena_PopScope(tmp_scope);
  }

  // load .gltf models
  {
    M_GLTF_Load("../res/models/Worker.gltf");
  }


  // exit
  M_LOG(M_LogIdk, "%s", (M.exit_code ? "Fail" : "Success"));
  return M.exit_code;
}
