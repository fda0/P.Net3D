// Base libraries
#define M_LOG_HEADER "[BAKER] "
#include "meta_util.h"
#include <SDL3_image/SDL_image.h>
#include "base_math.h"
#include "base_printer.h"
#include "base_parse.h"
#include "base_hash.h"

// Headers shared across baker and game
#include "bread_file_format.h"
#include "game_render.h"
#include "game_animation.h"
#include "game_asset_definitions.h"

// 3rd party libraries
#pragma warning(push, 2)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "bc7enc.h"
#pragma warning(pop)

// Baker headers
#include "baker_number_buffer.h"
#include "baker_entry.h"

// Baker implementations
#include "baker_bread_builder.c"
#include "baker_textures.c"
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

  // Init bc7enc texture compressor
  {
    bc7enc_compress_block_init();
    bc7enc_compress_block_params_init(&BAKER.tex.params);
    // There are params fields that we might want modify like m_uber_level.

    // Learn which one should be picked.
    // Perhaps diffuse needs perpecetual and data linear? Idk
    bc7enc_compress_block_params_init_linear_weights(&BAKER.tex.params);
    //bc7enc_compress_block_params_init_perceptual_weights(&BAKER.tex.params);
  }

  BREAD_Builder bb = BREAD_CreateBuilder(BAKER.tmp, Megabyte(64));

  // Process assets
  {
    ArenaScope tmp_check = Arena_PushScope(BAKER.tmp);

    // Load texture files
    {
      BAKER_CompressTexture(&bb, TEX_Bricks071);
    }

    // Load .gltf models
    {

      Quat rot_x = Quat_FromAxisAngle_RH(AxisV3_X(), 0.25f);
      Quat rot_y = Quat_FromAxisAngle_RH(AxisV3_Y(), 0.25f);
      Quat rot_z = Quat_FromAxisAngle_RH(AxisV3_Z(), 0.25f);
      Quat rot_xz = Quat_Mul(rot_z, rot_x);
      (void)rot_y;

      float scale = 40;
      BK_GLTF_ModelConfig config = {.scale = scale, .rot = rot_xz};

      BK_GLTF_Load(&bb, MODEL_Flag, "../res/models/Flag.glb",
                   (BK_GLTF_ModelConfig){1.f, rot_x, (V3){0,0,-4.5f}});
      BK_GLTF_Load(&bb, MODEL_Worker, "../res/models/Worker.gltf", config);
      BK_GLTF_Load(&bb, MODEL_Formal, "../res/models/Formal.gltf", config);
      BK_GLTF_Load(&bb, MODEL_Casual, "../res/models/Casual.gltf", config);

      config.scale = 4.f;
      config.rot = Quat_Identity();
      BK_GLTF_Load(&bb, MODEL_Tree, "../res/models/tree_low-poly/scene.gltf", config);
    }

    // Check that all sub-functions didn't leak memory on tmp arena
    M_Check(tmp_check.used == tmp_check.a->used);
    Arena_PopScope(tmp_check);
  }

  BREAD_FinalizeBuilder(&bb);
  BREAD_SaveToFile(&bb, "data.bread");

  // exit
  M_LOG(M_LogIdk, "%s", (M_LogState.error_count > 0 ? "Fail" : "Success"));
  return M_LogState.error_count;
}
