#define M_LOG_HEADER "[BAKER] "
#include "meta_util.h"
#include "base_math.h"
#include "base_printer.h"
#include "game_render.h"
#include "game_animation.h"

#include "baker_obj_loader.h"
#include "baker_entry.h"
#include "baker_print_parse.c"
#include "baker_obj_loader.c"

#define CGLTF_IMPLEMENTATION
#pragma warning(push, 2)
#include "cgltf.h"
#pragma warning(pop)

#include "baker_number_buffer.h"
#include "baker_gltf_loader.c"

static U8 tmp_arena_memory[Megabyte(64)];
static U8 cgltf_arena_memory[Megabyte(64)];

int main()
{
  // init
  BAKER.tmp = Arena_MakeInside(tmp_arena_memory, sizeof(tmp_arena_memory));
  BAKER.cgltf_arena = Arena_MakeInside(cgltf_arena_memory, sizeof(cgltf_arena_memory));

  M_LogState.reject_filter = M_LogObjDebug | M_LogGltfDebug;

  // load .obj models
  {
    ArenaScope tmp_scope = Arena_PushScope(BAKER.tmp);

    Printer pr_out = Pr_Alloc(BAKER.tmp, Megabyte(1));
    M_ParseObj("../res/models/teapot.obj", &pr_out, (M_ModelSpec){.scale = 10.f, .rot_x = 0.25f});
    M_ParseObj("../res/models/flag.obj", &pr_out, (M_ModelSpec){.scale = 0.1f, .rot_x = 0.25f, .rot_z = 0.25f});
    M_SaveFile("../gen/gen_models.h", Pr_AsS8(&pr_out));

    Arena_PopScope(tmp_scope);
  }

  // load .gltf models
  {
    ArenaScope tmp_scope = Arena_PushScope(BAKER.tmp);

    Printer pr_out = Pr_Alloc(BAKER.tmp, Megabyte(4));
    Printer pr_anim = Pr_Alloc(BAKER.tmp, Megabyte(4));
    //BK_GLTF_Load("../res/models/Worker.gltf", &pr_out, &pr_anim);
    BK_GLTF_Load("../res/models/Formal.gltf", &pr_out, &pr_anim);
    M_SaveFile("../gen/gen_models_gltf.h", Pr_AsS8(&pr_out));
    M_SaveFile("../gen/gen_animations.h", Pr_AsS8(&pr_anim));

    Arena_PopScope(tmp_scope);
  }


  // exit
  M_LOG(M_LogIdk, "%s", (M_LogState.error_count > 0 ? "Fail" : "Success"));
  return M_LogState.error_count;
}
