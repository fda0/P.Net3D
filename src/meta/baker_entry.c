// Base libraries
#define M_LOG_HEADER "[BAKER] "
#include "meta_util.h"
#include <SDL3_image/SDL_image.h>
#include "base_parse.h"
#include "base_hash.h"

// Headers shared across baker and game
#include "pie_file_format.h"
#include "game_render_world.h"
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

static Arena *Arena_Malloc(U64 size)
{
  return Arena_MakeInside(malloc(size), size);
}

int main()
{
  // Init
  BAKER.tmp = Arena_Malloc(Gigabyte(1));
  BAKER.cgltf_arena = Arena_Malloc(Gigabyte(1));
  M_LogState.reject_filter = M_GLTFDebug;

  // @todo There is a bug with BC7 currently where it doesn't encode alpha channel properly!
  //       I'm not yet sure if the bug is in BAKER or GAME.
  //       I'm planning to add .dds texture export option so I can watch my textures in some 3rd party viewer.
  //       This might be a bug in bc7end or my usage of it.
  //PIE_TexFormat tex_format = PIE_Tex_BC7_RGBA;
  PIE_TexFormat tex_format = PIE_Tex_R8G8B8A8;
  BK_TEX_Init(tex_format);

  BAKER.pie_builder = PIE_CreateBuilder(BAKER.tmp, Megabyte(256));

  // Process assets
  {
    Arena_Scope tmp_check = Arena_PushScope(BAKER.tmp);

    // Load .gltf models
    {
      Quat rot_x = Quat_FromAxisAngle_RH(AxisV3_X(), 0.25f);
      Quat rot_y = Quat_FromAxisAngle_RH(AxisV3_Y(), 0.25f);
      Quat rot_z = Quat_FromAxisAngle_RH(AxisV3_Z(), 0.25f);
      Quat rot_xz = Quat_Mul(rot_z, rot_x);
      (void)rot_y;

      BK_GLTF_Spec spec = {.height = 1.7f, .rot = rot_xz};
      // @todo detect duplicated skeleton & animations and reuse it
      BK_GLTF_Load("../res/models/UltimateModularWomen/Worker.gltf", spec);
      BK_GLTF_Load("../res/models/UltimateModularWomen/Formal.gltf", spec);
      BK_GLTF_Load("../res/models/UltimateModularWomen/Casual.gltf", spec);

      BK_GLTF_Load("../res/models/UniversalAnimationLibrary[Standard]/Dude.glb", spec);

      BK_GLTF_Load("../res/models/Flag.glb", (BK_GLTF_Spec){.height = 1.f, .rot = rot_x});
      BK_GLTF_Load("../res/models/tree_low-poly/scene.gltf", (BK_GLTF_Spec)
                   {.disable_z0 = true, .height = 5.f, .name = S8Lit("Tree")});
    }

    // Load texture files
    {
      // @todo adjust after adding tone mapping
      U32 spec_tiny = Color32_Grayf(0.05f);
      U32 spec_low = Color32_Grayf(0.1f);
      U32 spec_mid = Color32_Grayf(0.2f);
      U32 spec_big = Color32_Grayf(0.4f);
      BK_TEX_LoadPBRs(S8Lit("Bricks071"), spec_mid);
      BK_TEX_LoadPBRs(S8Lit("Bricks097"), spec_mid);
      BK_TEX_LoadPBRs(S8Lit("Clay002"), spec_big);
      BK_TEX_LoadPBRs(S8Lit("Grass004"), spec_tiny);
      BK_TEX_LoadPBRs(S8Lit("Ground037"), spec_tiny);
      BK_TEX_LoadPBRs(S8Lit("Ground068"), spec_tiny);
      BK_TEX_LoadPBRs(S8Lit("Ground078"), spec_tiny);
      BK_TEX_LoadPBRs(S8Lit("Leather011"), spec_big);
      BK_TEX_LoadPBRs(S8Lit("PavingStones067"), spec_low);
      BK_TEX_LoadPBRs(S8Lit("Tiles087"), spec_big);
      BK_TEX_LoadPBRs(S8Lit("Tiles101"), spec_big);
      BK_TEX_LoadPBRs(S8Lit("TestPBR001"), spec_mid);
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
