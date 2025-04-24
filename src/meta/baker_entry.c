#define M_LOG_HEADER "[BAKER] "
#include "meta_util.h"
#include "base_math.h"
#include "base_printer.h"
#include "base_parse.h"
#include "game_render.h"
#include "game_animation.h"

#include "baker_obj_loader.h"
#include "baker_gltf_loader.h"
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
    ArenaScope scratch = Arena_PushScope(BAKER.tmp);

    Printer pr_out = Pr_Alloc(scratch.a, Megabyte(1));
    M_ParseObj("../res/models/teapot.obj", &pr_out, (M_ModelSpec){.scale = 10.f, .rot_x = 0.25f});
    M_ParseObj("../res/models/flag.obj", &pr_out, (M_ModelSpec){.scale = 0.1f, .rot_x = 0.25f, .rot_z = 0.25f});
    M_SaveFile("../gen/gen_models.h", Pr_AsS8(&pr_out));

    Arena_PopScope(scratch);
  }

  // load .gltf models
  {
    ArenaScope scratch = Arena_PushScope(BAKER.tmp);
    Printer pr_out = Pr_Alloc(scratch.a, Megabyte(4));
    Printer pr_anim = Pr_Alloc(scratch.a, Megabyte(4));

    Mat4 rot_xz = Mat4_Rotation_RH((V3){1,0,0}, 0.25f);
    rot_xz = Mat4_Mul(Mat4_Rotation_RH((V3){0,0,1}, 0.25f), rot_xz);
    BK_GLTF_Config config =
    {
      .scale = 40,
      .rot = rot_xz,
    };

    //BK_GLTF_Load("../res/models/Worker.gltf", &pr_out, &pr_anim, config);
    //BK_GLTF_Load("../res/models/Formal.gltf", &pr_out, &pr_anim, config);

    config.scale = 4.f;
    config.rot = Mat4_Identity();
    BK_GLTF_Load("../res/models/tree_low-poly/scene.gltf", &pr_out, &pr_anim, config);


    M_SaveFile("../gen/gen_models_gltf.h", Pr_AsS8(&pr_out));
    M_SaveFile("../gen/gen_animations.h", Pr_AsS8(&pr_anim));

    Arena_PopScope(scratch);
  }


  // exit
  M_LOG(M_LogIdk, "%s", (M_LogState.error_count > 0 ? "Fail" : "Success"));
  return M_LogState.error_count;
}
