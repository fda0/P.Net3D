// Base libraries
#define M_LOG_HEADER "[BAKER] "
#include "meta_util.h"
#include "base_math.h"
#include "base_printer.h"
#include "base_parse.h"
#include "base_hash.h"

// Headers shared across baker and game
#include "bread_file_format.h"
#include "game_render.h"
#include "game_animation.h"
#include "game_asset_definitions.h"

// Baker headers
#include "baker_number_buffer.h"
#include "baker_entry.h"

// 3rd party libraries
#pragma warning(push, 2)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "bc7enc.h"
#pragma warning(pop)

// Baker implementations
#include "baker_bread_builder.c"
#include "baker_gltf_loader.c"

// Memory allocations
static U8 tmp_arena_memory[Megabyte(256)];
static U8 cgltf_arena_memory[Megabyte(64)];

int main()
{
  // Init
  BAKER.tmp = Arena_MakeInside(tmp_arena_memory, sizeof(tmp_arena_memory));
  BAKER.cgltf_arena = Arena_MakeInside(cgltf_arena_memory, sizeof(cgltf_arena_memory));
  M_LogState.reject_filter = M_LogObjDebug | M_LogGltfDebug;

  bc7enc_compress_block_init();

  BREAD_Builder bb = BREAD_CreateBuilder(BAKER.tmp, Megabyte(64));

  // Load .gltf models
  {
    ArenaScope scratch = Arena_PushScope(BAKER.tmp);

    Quat rot_x = Quat_FromAxisAngle_RH(AxisV3_X(), 0.25f);
    Quat rot_y = Quat_FromAxisAngle_RH(AxisV3_Y(), 0.25f);
    Quat rot_z = Quat_FromAxisAngle_RH(AxisV3_Z(), 0.25f);
    Quat rot_xz = Quat_Mul(rot_z, rot_x);
    (void)rot_y;

    float scale = 40;
    BK_GLTF_ModelConfig config = {.scale = scale, .rot = rot_xz};

    BK_GLTF_Load(MODEL_Flag, "flag", "../res/models/Flag.glb", &bb,
                 (BK_GLTF_ModelConfig){1.f, rot_x, (V3){0,0,-4.5f}});
    BK_GLTF_Load(MODEL_Worker, "", "../res/models/Worker.gltf", &bb, config);
    BK_GLTF_Load(MODEL_Formal, "", "../res/models/Formal.gltf", &bb, config);
    BK_GLTF_Load(MODEL_Casual, "", "../res/models/Casual.gltf", &bb, config);

    config.scale = 4.f;
    config.rot = Quat_Identity();
    BK_GLTF_Load(MODEL_Tree, "Tree", "../res/models/tree_low-poly/scene.gltf", &bb, config);

    Arena_PopScope(scratch);
  }

  BREAD_FinalizeBuilder(&bb);
  BREAD_SaveToFile(&bb, "data.bread");

  // exit
  M_LOG(M_LogIdk, "%s", (M_LogState.error_count > 0 ? "Fail" : "Success"));
  return M_LogState.error_count;
}
