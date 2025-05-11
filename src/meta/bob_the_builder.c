// bob_the_builder - custom build program.
// Features:
// - Flexible command-line interface that does exactly what the author (me) wants.
// - Measures build times.
// - Caches asset preprocessor (baker) output.
// - Hides output spam.
#include "bob_the_builder_impl.h"

int main(I32 args_count, char **args)
{
  BOB_Init(args_count, args);

  if (BOB.sdl.enabled == BOB_TRUE)
  {
    BOB_Compilation comp = BOB.sdl;
    BOB_MarkSection("SDL", comp, (BOB_SectionConfig){.is_compilation = true});
    bool release = comp.release == BOB_TRUE;

    // SDL build docs: https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md
    // @todo(mg): pass gcc/clang flag to SDL (is that even possible?)

    BOB_PrinterOnStack(stage1);
    Pr_Cstr(&stage1, "cmake -S . -B build/win");
    if (release)
      Pr_Cstr(&stage1, " -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=\"/arch:AVX2\" -DCMAKE_CXX_FLAGS=\"/arch:AVX2\"");

    BOB_PrinterOnStack(stage2);
    Pr_Cstr(&stage2, "cmake --build build/win");
    if (release)
      Pr_Cstr(&stage2, " --config Release");

    // Compile main SDL3 target
    BOB_PrinterOnStack(cmd);
    Pr_Cstr(&cmd, "cd ../libs/SDL && ");
    Pr_Printer(&cmd, &stage1);
    Pr_Cstr(&cmd, " -DSDL_STATIC=ON && ");
    Pr_Printer(&cmd, &stage2);
    I32 r = BOB_RunPrinter(&cmd);
    BOB_CheckError(r);

    // Compile SDL3 additional libs
    Pr_Cstr(&stage1, " -DBUILD_SHARED_LIBS=OFF -DSDL3_DIR=../SDL/build/win");

    Pr_Reset(&cmd);
    Pr_Cstr(&cmd, "cd ../libs/SDL_ttf && ");
    Pr_Printer(&cmd, &stage1);
    Pr_Cstr(&cmd, " && ");
    Pr_Printer(&cmd, &stage2);
    r = BOB_RunPrinter(&cmd);
    BOB_CheckError(r);

    Pr_Reset(&cmd);
    Pr_Cstr(&cmd, "cd ../libs/SDL_net && ");
    Pr_Printer(&cmd, &stage1);
    Pr_Cstr(&cmd, " && ");
    Pr_Printer(&cmd, &stage2);
    r = BOB_RunPrinter(&cmd);
    BOB_CheckError(r);

    Pr_Reset(&cmd);
    Pr_Cstr(&cmd, "cd ../libs/SDL_image && ");
    Pr_Printer(&cmd, &stage1);
    Pr_Cstr(&cmd, " -DSDLIMAGE_VENDORED=OFF && ");
    Pr_Printer(&cmd, &stage2);
    r = BOB_RunPrinter(&cmd);
    BOB_CheckError(r);
  }

  if (BOB.shaders.enabled)
  {
    // cache check
    bool rebuild = true;

    bool is_cached = false;
    const char *cache_paths[] = { "../src/game/shader*" };
    BOB_MarkSection("Shaders", BOB.shaders, (BOB_SectionConfig){.check_if_cached = &is_cached,
                                                                .cache_paths_count = ArrayCount(cache_paths),
                                                                .cache_paths = cache_paths});

    if (!is_cached)
    {
      const char *cmd =
        // Clean gen directory
        "(if exist ..\\gen\\ rmdir /q /s ..\\gen\\) && "
        "mkdir ..\\gen\\ && "

        // Precompile shaders
        "dxc ../src/game/shader_world.hlsl /E World_DxShaderRigidVS   /T vs_6_0 /D IS_RIGID=1    /Fh ../gen/gen_shader_rigid.vert.h && "
        "dxc ../src/game/shader_world.hlsl /E World_DxShaderRigidPS   /T ps_6_0 /D IS_RIGID=1    /Fh ../gen/gen_shader_rigid.frag.h && "
        "dxc ../src/game/shader_world.hlsl /E World_DxShaderSkinnedVS /T vs_6_0 /D IS_SKINNED=1  /Fh ../gen/gen_shader_skinned.vert.h && "
        "dxc ../src/game/shader_world.hlsl /E World_DxShaderSkinnedPS /T ps_6_0 /D IS_SKINNED=1  /Fh ../gen/gen_shader_skinned.frag.h && "
        "dxc ../src/game/shader_world.hlsl /E World_DxShaderMeshVS    /T vs_6_0 /D IS_TEXTURED=1 /Fh ../gen/gen_shader_mesh.vert.h && "
        "dxc ../src/game/shader_world.hlsl /E World_DxShaderMeshPS    /T ps_6_0 /D IS_TEXTURED=1 /Fh ../gen/gen_shader_mesh.frag.h && "
        "dxc ../src/game/shader_ui.hlsl    /E UI_DxShaderVS           /T vs_6_0                  /Fh ../gen/gen_shader_ui.vert.h && "
        "dxc ../src/game/shader_ui.hlsl    /E UI_DxShaderPS           /T ps_6_0                  /Fh ../gen/gen_shader_ui.frag.h";
      I32 r = BOB_Run(cmd);
      if (r) BOB_SaveCacheHash("Shaders", 0);
      BOB_CheckError(r);
    }
  }

  if (BOB.math.enabled)
  {
    BOB_Compilation comp = BOB.math;
    BOB_MarkSection("Math", comp, (BOB_SectionConfig){.is_compilation = true});

    BOB_PrinterOnStack(cmd);
    BOB_BuildCommand(&cmd, (BOB_BuildConfig){.inputs = "../src/meta/codegen_math.c",
                                             .output = "codegen_math.exe",
                                             .comp = comp,
                                             .sdl3 = true});
    I32 r = BOB_RunPrinter(&cmd);
    BOB_CheckError(r);

    if (comp.run != BOB_FALSE)
    {
      BOB_MarkSection("Math-run", comp, (BOB_SectionConfig){});
      r = BOB_Run("codegen_math.exe");
      BOB_CheckError(r);
    }
  }

  if (BOB.baker.enabled)
  {
    BOB_Compilation comp = BOB.baker;
    if (comp.release == BOB_DEFAULT) // Default for baker is release mode
      comp.release = BOB_TRUE;

    bool run_baker = comp.run != BOB_FALSE;

    // cache check
    bool is_cached = false;
    const char *cache_paths[] =
    {
      "../src/base",
      "../src/meta",
    };

    BOB_MarkSection("Baker", comp, (BOB_SectionConfig){.is_compilation = true,
                                                       .check_if_cached = &is_cached,
                                                       .cache_paths_count = ArrayCount(cache_paths),
                                                       .cache_paths = cache_paths,
                                                       .force_invalidate_cache = !run_baker});

    if (!is_cached || !BOB_FileExists("data.bread"))
    {
      I32 r = BOB_RunIcon(comp, "cook");
      BOB_CheckError(r);

      BOB_PrinterOnStack(cmd);
      BOB_BuildCommand(&cmd, (BOB_BuildConfig){.inputs = "../src/meta/baker_entry.c ../libs/bc7enc.c",
                                               .link_inputs = "cook.res",
                                               .output = "baker.exe",
                                               .comp = comp,
                                               .sdl3 = true,
                                               .sdl3_image = true});
      r = BOB_RunPrinter(&cmd);
      BOB_CheckError(r);

      if (run_baker)
      {
        BOB_MarkSection("Baker-run", comp, (BOB_SectionConfig){});
        r = BOB_Run("baker.exe");
        if (r) BOB_SaveCacheHash("Baker", 0);
        BOB_CheckError(r);
      }
    }
  }

  if (BOB.justgame.enabled)
  {
    BOB_Compilation comp = BOB.justgame;
    if (comp.show_out == BOB_DEFAULT) comp.show_out = BOB_TRUE; // default to showing output for game target
    BOB_MarkSection("P", comp, (BOB_SectionConfig){.is_compilation = true});

    // Produce icon file
    I32 r = BOB_RunIcon(comp, "coin");
    BOB_CheckError(r);

    // Compile game
    BOB_PrinterOnStack(cmd);
    BOB_BuildCommand(&cmd, (BOB_BuildConfig){.inputs = "../src/game/game_sdl_entry.c",
                                             .link_inputs = "coin.res",
                                             .output = "p.exe",
                                             .comp = comp,
                                             .gui = true,
                                             .sdl3 = true,
                                             .sdl3_net = true,
                                             .sdl3_ttf = true,
                                             .sdl3_image = false});
    r = BOB_RunPrinter(&cmd);
    BOB_CheckError(r);
  }

  if (!BOB.did_build)
  {
    fprintf(BOB_err, "[BOB] No build target was provided. To build everything use: build.bat release all\n");
    exit(1);
  }

  BOB_MarkSection(0, (BOB_Compilation){}, (BOB_SectionConfig){});
  fprintf(BOB_out, "[BOB] Success. %s\n", Pr_AsCstr(&BOB.log));
  return 0;
}
