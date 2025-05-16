// Base libraries
#define M_LOG_HEADER "[BAKER] "
#include "meta_util.h"
#include <SDL3_image/SDL_image.h>
#include "base_math.h"
#include "base_printer.h"
#include "base_parse.h"
#include "base_hash.h"

// Headers shared across baker and game
#include "pie_file_format.h"
#include "game_render.h"
#include "game_asset_keys.h"
#include "game_type_info.h"

// 3rd party libraries
#pragma warning(push, 2)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "bc7enc.h"
#pragma warning(pop)

// Baker headers
#include "baker_number_buffer.h"
#include "baker_entry.h"

// Pie
static S8 PIE_File()
{
  return Pr_AsS8(&BAKER.pie_builder.file);
}

static void BK_SetDefaultsPIEMaterial(PIE_Material *pie_material)
{
  pie_material->params.diffuse = Color32_RGBi(128,128,128);
  pie_material->params.specular = Color32_RGBf(0.05f, 0.02f, 0.01f);
  pie_material->params.roughness = 0.5f;
}

#include "pie_loader.h"
#include "pie_builder.c"

// Baker implementations
#include "baker_textures.c"
#include "baker_gltf_models.c"

// Memory allocations
static U8 tmp_arena_memory[Megabyte(512)];
static U8 cgltf_arena_memory[Megabyte(32)];

int main()
{
  // Init
  BAKER.tmp = Arena_MakeInside(tmp_arena_memory, sizeof(tmp_arena_memory));
  BAKER.cgltf_arena = Arena_MakeInside(cgltf_arena_memory, sizeof(cgltf_arena_memory));
  M_LogState.reject_filter = M_GLTFDebug;

  BK_TEX_Init();

  BAKER.pie_builder = PIE_CreateBuilder(BAKER.tmp, Megabyte(256));

  // Process assets
  {
    ArenaScope tmp_check = Arena_PushScope(BAKER.tmp);

    // Load texture files
    {
      //PIE_TexFormat format = PIE_Tex_R8G8B8A8;
      PIE_TexFormat format = PIE_Tex_BC7_RGBA;

      BK_TEX_Load(S8Lit("Leather011"), format);
      BK_TEX_Load(S8Lit("PavingStones067"), format);
      BK_TEX_Load(S8Lit("Tiles101"), format);
      BK_TEX_Load(S8Lit("TestPBR001"), format);
      BK_TEX_Load(S8Lit("Tree0Bark"), format);
      BK_TEX_Load(S8Lit("Tree0eaves"), format);
      BK_TEX_Load(S8Lit("Bricks071"), format);
      BK_TEX_Load(S8Lit("Bricks097"), format);
      BK_TEX_Load(S8Lit("Grass004"), format);
      BK_TEX_Load(S8Lit("Ground037"), format);
      BK_TEX_Load(S8Lit("Ground068"), format);
      BK_TEX_Load(S8Lit("Ground078"), format);
      BK_TEX_Load(S8Lit("Tree0Material"), format);
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

      BK_GLTF_Load(MODEL_Flag, "../res/models/Flag.glb",
                   (BK_GLTF_ModelConfig){1.f, rot_x, (V3){0,0,-4.5f}});
      BK_GLTF_Load(MODEL_Worker, "../res/models/Worker.gltf", config);
      BK_GLTF_Load(MODEL_Formal, "../res/models/Formal.gltf", config);
      BK_GLTF_Load(MODEL_Casual, "../res/models/Casual.gltf", config);

      config.scale = 4.f;
      config.rot = Quat_Identity();
      BK_GLTF_Load(MODEL_Tree, "../res/models/tree_low-poly/scene.gltf", config);
    }

    // Check that all sub-functions didn't leak memory on tmp arena
    M_Check(tmp_check.used == tmp_check.a->used);
    Arena_PopScope(tmp_check);
  }

  PIE_FinalizeBuilder();
  PIE_SaveToFile("data.pie");

  // exit
  M_LOG(M_Idk, "%s", (M_LogState.error_count > 0 ? "Fail" : "Success"));
  return M_LogState.error_count;
}
