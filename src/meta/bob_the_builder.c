// Simple build program custom built for this project.
// Features:
// - Flexible command-line interface that does exactly what the author (me) wants.
// - Measures build times.
// - Caches asset preprocessor (baker) output.
#include <stdio.h>
#include "base_types.h"
#include "base_string.h"
#define PRINTER_SKIP_ARENA
#define PRINTER_SKIP_MATH
#include "base_printer.h"
#define BOB_PrinterOnStack(p) Pr_MakeOnStack(p, Kilobyte(32))

typedef enum
{
  BOB_DEFAULT,
  BOB_FALSE,
  BOB_TRUE,
} BOB_B3;

typedef enum
{
  BOB_CC_DEFAULT,
  BOB_CC_MSVC,
  BOB_CC_CLANG,
} BOB_Compiler;

typedef struct
{
  BOB_B3 enabled;
  BOB_Compiler compiler;
  BOB_B3 release;
  BOB_B3 asan;
  BOB_B3 norun;
  BOB_B3 nocache;
} BOB_Compilation;

typedef struct
{
  // Tracks currently set options
  BOB_Compilation comp;

  // Compilation targets
  BOB_Compilation sdl;
  BOB_Compilation shaders;
  BOB_Compilation math;
  BOB_Compilation baker;
  BOB_Compilation justgame;

  // Target groups
  BOB_Compilation game; // shaders, baker, justgame
  BOB_Compilation all; // everything

  // Other
  bool did_build;
} BOB_State;
static BOB_State BOB;

static void BOB_CompInherit(BOB_Compilation *parent, BOB_Compilation *child)
{
  if (child->enabled == BOB_DEFAULT)     child->enabled = parent->enabled;
  if (child->compiler == BOB_CC_DEFAULT) child->compiler = parent->compiler;
  if (child->release == BOB_DEFAULT)     child->release = parent->release;
  if (child->asan == BOB_DEFAULT)        child->asan = parent->asan;
  if (child->norun == BOB_DEFAULT)       child->norun = parent->norun;
  if (child->nocache == BOB_DEFAULT)     child->nocache = parent->nocache;
}

static int BOB_System(const char *command)
{
  fprintf(stdout, "[BOB] RUNNING: %s\n", command);
  return system(command);
}
static int BOB_SystemPrinter(Printer *p)
{
  const char *command = Pr_AsCstr(p);
  if (p->err)
  {
    fprintf(stderr, "[BOB] ERROR: Internal printer error\n");
    exit(1);
  }
  return BOB_System(command);
}

static void BOB_CheckError(I32 error_code)
{
  if (error_code)
  {
    fprintf(stderr, "[BOB] ERROR: Exiting with code %d\n", error_code);
    exit(error_code);
  }
}

typedef struct
{
  bool release;
  bool clang;
  bool asan;
  bool gui;
  bool sdl3;
  bool sdl3_net;
  bool sdl3_ttf;
  bool sdl3_image;
} BOB_BuildConfig;

static void BOB_BuildCommand(Printer *cmd, BOB_BuildConfig config)
{
  // Compile/Link definitions
  const char *include_paths =
    "-I../src/base/ -I../src/game/ -I../src/meta/ -I../gen/ -I../libs/ "
    "-I../libs/SDL/include/ -I../libs/SDL_ttf/include/ -I../libs/SDL_net/include/ -I../libs/SDL_image/include/";

  const char *sdl_path_debug = "build/win/Debug/";
  const char *sdl_path_release = "build/win/Release/";

  const char *enable_asan  = "-fsanitize=address ";

  // Compile/Link definitions per compiler
  const char *msvc_exe       = "cl ";
  const char *msvc_debug     = "/DBUILD_DEBUG=0 /Od /Ob1 /MDd ";
  const char *msvc_release   = "/DBUILD_DEBUG=1 /O2 /MD ";
  const char *msvc_common    = "/nologo /FC /Z7 /W4 /wd4244 /wd4201 /wd4324 /std:c17 ";
  const char *msvc_link      = "/link /MANIFEST:EMBED /INCREMENTAL:NO /pdbaltpath:%_PDB% ";
  const char *msvc_link_libs = "User32.lib Advapi32.lib Shell32.lib Gdi32.lib Version.lib OleAut32.lib Imm32.lib Ole32.lib Cfgmgr32.lib Setupapi.lib Winmm.lib Ws2_32.lib Iphlpapi.lib ";
  const char *msvc_link_gui  = "/SUBSYSTEM:WINDOWS ";
  const char *msvc_out       = "/out: ";

  const char *clang_exe       = "clang ";
  const char *clang_debug     = "-DBUILD_DEBUG=0 -g -O0 ";
  const char *clang_release   = "-DBUILD_DEBUG=1 -g -O2 ";
  const char *clang_common    = "-fdiagnostics-absolute-paths -Wall -Wno-missing-braces -Wno-unused-function -Wno-microsoft-static-assert -Wno-c2x-extensions ";
  const char *clang_link      = "-fuse-ld=lld -Xlinker /MANIFEST:EMBED -Xlinker /pdbaltpath:%_PDB% ";
  const char *clang_link_libs = "-lUser32.lib -lAdvapi32.lib -lShell32.lib -lGdi32.lib -lVersion.lib -lOleAut32.lib -lImm32.lib -lOle32.lib -lCfgmgr32.lib -lSetupapi.lib -lWinmm.lib -lWs2_32.lib -lIphlpapi.lib ";
  const char *clang_link_gui  = "-Xlinker /SUBSYSTEM:WINDOWS ";
  const char *clang_out       = "-o ";

  // Select compiler
  const char *exe = config.clang ? clang_exe : msvc_exe;
  const char *debug = config.clang ? clang_debug : msvc_debug;
  const char *release = config.clang ? clang_release : msvc_release;
  const char *common = config.clang ? clang_common : msvc_common;
  const char *link = config.clang ? clang_link : msvc_link;
  const char *link_libs = config.clang ? clang_link_libs : msvc_link_libs;
  const char *link_gui = config.clang ? clang_link_gui : msvc_link_gui;
  const char *out = config.clang ? clang_out : msvc_out;

  // Select optimizations
  const char *opt_mode = config.release ? release : debug;
  const char *sdl_path = config.release ? sdl_path_release : sdl_path_debug;

  // Build command string
  Pr_Cstr(cmd, exe);
  Pr_Cstr(cmd, opt_mode);
  Pr_Cstr(cmd, common);

  // @todo Inject git hash predefine
  // for /f %%i in ('call git describe --always --dirty') do set compile=%compile% -DBUILD_GIT_HASH=\"%%i\"

  if (config.asan)
    Pr_Cstr(cmd, enable_asan);

  Pr_Cstr(cmd, link);

  if (config.gui)
    Pr_Cstr(cmd, link_gui);

  Pr_Cstr(cmd, link_libs);

  if (config.sdl3)
  {
    Pr_Cstr(cmd, "../libs/SDL/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3-static.lib");
  }
  if (config.sdl3_net)
  {
    Pr_Cstr(cmd, "../libs/SDL_net/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3_net-static.lib");
  }
  if (config.sdl3_ttf)
  {
    Pr_Cstr(cmd, "../libs/SDL_ttf/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3_ttf-static.lib");
  }
  if (config.sdl3_image)
  {
    Pr_Cstr(cmd, "../libs/SDL_image/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3_image-static.lib");
  }

  Pr_Cstr(cmd, out);
}

//
int main(I32 args_count, char **args)
{
  BOB.comp.enabled = BOB_TRUE;

  for (I32 i = 1; i < args_count; i += 1)
  {
    S8 arg = S8_FromCstr(args[i]);
    S8_MatchFlags f = S8Match_CaseInsensitive;

    // Options
    if      (S8_Match(arg, S8Lit("release"), f)) BOB.comp.release = BOB_TRUE;
    else if (S8_Match(arg, S8Lit("debug"), f))   BOB.comp.release = BOB_FALSE;
    else if (S8_Match(arg, S8Lit("msvc"), f))    BOB.comp.compiler = BOB_CC_MSVC;
    else if (S8_Match(arg, S8Lit("clang"), f))   BOB.comp.compiler = BOB_CC_CLANG;

    // Targets
    else if (S8_Match(arg, S8Lit("sdl"), f))      BOB.sdl = BOB.comp;
    else if (S8_Match(arg, S8Lit("shaders"), f))  BOB.shaders = BOB.comp;
    else if (S8_Match(arg, S8Lit("math"), f))     BOB.math = BOB.comp;
    else if (S8_Match(arg, S8Lit("baker"), f))    BOB.baker = BOB.comp;
    else if (S8_Match(arg, S8Lit("justgame"), f)) BOB.justgame = BOB.comp;

    // Target groups
    else if (S8_Match(arg, S8Lit("game"), f))     BOB.game = BOB.comp;
    else if (S8_Match(arg, S8Lit("all"), f))      BOB.all = BOB.comp;

    // Unknown
    else
      fprintf(stderr, "[BOB] WARNING: Skipping unknown command line option: %.*s\n", S8Print(arg));
  }

  // Transfer options from target groups to targets
  BOB_CompInherit(&BOB.game, &BOB.baker);
  BOB_CompInherit(&BOB.game, &BOB.shaders);
  BOB_CompInherit(&BOB.game, &BOB.justgame);

  BOB_CompInherit(&BOB.all, &BOB.sdl);
  BOB_CompInherit(&BOB.all, &BOB.math);
  BOB_CompInherit(&BOB.all, &BOB.baker);
  BOB_CompInherit(&BOB.all, &BOB.shaders);
  BOB_CompInherit(&BOB.all, &BOB.justgame);

  //
  if (BOB.sdl.enabled == BOB_TRUE)
  {
    bool release = BOB.sdl.release == BOB_TRUE;
    fprintf(stdout, "[BOB] Target SDL; %s\n",
            release ? "release" : "debug");

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
    I32 r = BOB_SystemPrinter(&cmd);
    BOB_CheckError(r);

    // Compile SDL3 additional libs
    Pr_Cstr(&stage1, " -DBUILD_SHARED_LIBS=OFF -DSDL3_DIR=../SDL/build/win");

    Pr_Reset(&cmd);
    Pr_Cstr(&cmd, "cd ../libs/SDL_ttf && ");
    Pr_Printer(&cmd, &stage1);
    Pr_Cstr(&cmd, " && ");
    Pr_Printer(&cmd, &stage2);
    r = BOB_SystemPrinter(&cmd);
    BOB_CheckError(r);

    Pr_Reset(&cmd);
    Pr_Cstr(&cmd, "cd ../libs/SDL_net && ");
    Pr_Printer(&cmd, &stage1);
    Pr_Cstr(&cmd, " && ");
    Pr_Printer(&cmd, &stage2);
    r = BOB_SystemPrinter(&cmd);
    BOB_CheckError(r);

    Pr_Reset(&cmd);
    Pr_Cstr(&cmd, "cd ../libs/SDL_image && ");
    Pr_Printer(&cmd, &stage1);
    Pr_Cstr(&cmd, " -DSDLIMAGE_VENDORED=OFF && ");
    Pr_Printer(&cmd, &stage2);
    r = BOB_SystemPrinter(&cmd);
    BOB_CheckError(r);

    BOB.did_build = true;
  }

  if (BOB.shaders.enabled)
  {
    fprintf(stdout, "[BOB] Target shaders\n");

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
    BOB_System(cmd);

    BOB.did_build = true;
  }

  if (BOB.math.enabled)
  {
    // @todo
  }

  if (BOB.baker.enabled)
  {
    // @todo
  }

  if (BOB.justgame.enabled)
  {
    bool release = BOB.justgame.release == BOB_TRUE;
    bool clang = BOB.justgame.compiler == BOB_CC_CLANG;
    bool asan = BOB.justgame.asan == BOB_TRUE;
    fprintf(stdout, "[BOB] Target justgame; %s; %s; %s\n",
            release ? "release" : "debug",
            clang ? "clang" : "msvc",
            asan ? "asan-on" : "asan-off");

    // Produce icon file
    BOB_PrinterOnStack(cmd);
    Pr_Cstr(&cmd, clang ? "llvm-rc" : "rc");
    Pr_Cstr(&cmd, " /nologo /fo icon.res ../res/ico/icon.rc");
    I32 r = BOB_SystemPrinter(&cmd);
    BOB_CheckError(r);

    // Compile game
    Pr_Reset(&cmd);
    BOB_BuildCommand(&cmd, (BOB_BuildConfig){.release = release,
                                             .clang = clang,
                                             .asan = asan,
                                             .gui = true,
                                             .sdl3 = true,
                                             .sdl3_net = true,
                                             .sdl3_ttf = true,
                                             .sdl3_image = true});
    r = BOB_SystemPrinter(&cmd);
    BOB_CheckError(r);

    BOB.did_build = true;
  }

  if (!BOB.did_build)
  {
    fprintf(stderr, "[BOB] No build target was provided. To build everything use: build.bat release all\n");
    BOB_CheckError(1);
  }

  fprintf(stdout, "[BOB] Success\n");
  return 0;
}
