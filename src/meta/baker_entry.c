#define M_LOG_HEADER "[BAKER] "
// .h
#include "meta_util.h"
#include "base_math.h"
#include "base_printer.h"
#include "base_parse.h"
#include "game_render.h"
#include "game_animation.h"

#include "baker_number_buffer.h"
#include "baker_gltf_loader.h"
#include "baker_entry.h"

// .c
#include "baker_print_parse.c"

#define CGLTF_IMPLEMENTATION
#pragma warning(push, 2)
#include "cgltf.h"
#pragma warning(pop)

#include "baker_gltf_loader.c"

//
static U8 tmp_arena_memory[Megabyte(256)];
static U8 cgltf_arena_memory[Megabyte(64)];

int main()
{
  // init
  BAKER.tmp = Arena_MakeInside(tmp_arena_memory, sizeof(tmp_arena_memory));
  BAKER.cgltf_arena = Arena_MakeInside(cgltf_arena_memory, sizeof(cgltf_arena_memory));
  M_LogState.reject_filter = M_LogObjDebug | M_LogGltfDebug;

  // load .gltf models
  {
    ArenaScope scratch = Arena_PushScope(BAKER.tmp);
    Printer pr_out = Pr_Alloc(scratch.a, Megabyte(32));
    Printer pr_anim = Pr_Alloc(scratch.a, Megabyte(8));

    Quat rot_x = Quat_FromAxisAngle_RH(AxisV3_X(), 0.25f);
    Quat rot_y = Quat_FromAxisAngle_RH(AxisV3_Y(), 0.25f);
    Quat rot_z = Quat_FromAxisAngle_RH(AxisV3_Z(), 0.25f);
    Quat rot_xz = Quat_Mul(rot_z, rot_x);
    (void)rot_y;

    float scale = 40;
    BK_GLTF_Config config = {.scale = scale, .rot = rot_xz};

    BK_GLTF_Load("flag", "../res/models/Flag.glb", &pr_out, &pr_anim, (BK_GLTF_Config){1.f, rot_x});
    BK_GLTF_Load("", "../res/models/Worker.gltf", &pr_out, &pr_anim, config);
    BK_GLTF_Load("", "../res/models/Formal.gltf", &pr_out, &pr_anim, config);

    config.scale = 4.f;
    config.rot = Quat_Identity();
    BK_GLTF_Load("Tree", "../res/models/tree_low-poly/scene.gltf", &pr_out, &pr_anim, config);


    M_SaveFile("../gen/gen_models_gltf.h", Pr_AsS8(&pr_out));
    M_SaveFile("../gen/gen_animations.h", Pr_AsS8(&pr_anim));

    Arena_PopScope(scratch);
  }


  // exit
  M_LOG(M_LogIdk, "%s", (M_LogState.error_count > 0 ? "Fail" : "Success"));
  return M_LogState.error_count;
}
