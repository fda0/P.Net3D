// Simple custom build program.
// Features:
// - measures build time
// - caches some stages like asset preprocessor (baker) output
#include <stdio.h>
#include "base_types.h"
#include "base_string.h"
#define PRINTER_SKIP_ARENA
#define PRINTER_SKIP_MATH
#include "base_printer.h"

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
} BOB_Compilation;

typedef struct
{
  // Tracks currently set options
  BOB_Compilation comp;

  // Compilation targets
  BOB_Compilation sdl;
  BOB_Compilation baker;
  BOB_Compilation shaders;
  BOB_Compilation justgame;

  // Target groups
  BOB_Compilation game; // baker, shaders, justgame
  BOB_Compilation all; // everything
} BOB_State;
static BOB_State BOB;

static void BOB_CompInherit(BOB_Compilation *parent, BOB_Compilation *child)
{
  if (child->enabled == BOB_DEFAULT)     child->enabled = parent->enabled;
  if (child->compiler == BOB_CC_DEFAULT) child->compiler = parent->compiler;
  if (child->release == BOB_DEFAULT)     child->release = parent->release;
  if (child->asan == BOB_DEFAULT)        child->asan = parent->asan;
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
    fprintf(stderr, "[BOB] Internal printer error\n");
    exit(1);
  }
  return BOB_System(command);
}

static void BOB_CheckError(I32 error_code)
{
  if (error_code)
  {
    fprintf(stderr, "[BOB] Exiting with error %d\n", error_code);
    exit(error_code);
  }
}

static void BOB_Build(BOB_Compilation comp, const char *name)
{
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
    else if (S8_Match(arg, S8Lit("baker"), f))    BOB.baker = BOB.comp;
    else if (S8_Match(arg, S8Lit("shaders"), f))  BOB.shaders = BOB.comp;
    else if (S8_Match(arg, S8Lit("justgame"), f)) BOB.justgame = BOB.comp;

    // Target groups
    else if (S8_Match(arg, S8Lit("game"), f))     BOB.game = BOB.comp;
    else if (S8_Match(arg, S8Lit("all"), f))      BOB.all = BOB.comp;

    // Unknown
    else
      fprintf(stderr, "[BOB] Skipping unknown option: %.*s\n", S8Print(arg));
  }

  // Transfer options from target groups to targets
  BOB_CompInherit(&BOB.game, &BOB.baker);
  BOB_CompInherit(&BOB.game, &BOB.shaders);
  BOB_CompInherit(&BOB.game, &BOB.justgame);

  BOB_CompInherit(&BOB.all, &BOB.sdl);
  BOB_CompInherit(&BOB.all, &BOB.baker);
  BOB_CompInherit(&BOB.all, &BOB.shaders);
  BOB_CompInherit(&BOB.all, &BOB.justgame);

  //
  if (BOB.sdl.enabled == BOB_TRUE)
  {
    // SDL build docs: https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md
    // @todo(mg): pass gcc/clang flag to SDL (is that even possible?)

    Pr_MakeOnStack(stage1, Kilobyte(1));
    Pr_Cstr(&stage1, "cmake -S . -B build/win");
    if (BOB.sdl.release == BOB_TRUE)
      Pr_Cstr(&stage1, " -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=\"/arch:AVX2\" -DCMAKE_CXX_FLAGS=\"/arch:AVX2\"");

    Pr_MakeOnStack(stage2, Kilobyte(1));
    Pr_Cstr(&stage2, "cmake --build build/win");
    if (BOB.sdl.release == BOB_TRUE)
      Pr_Cstr(&stage2, " --config Release");

    // Compile main SDL3 target
    Pr_MakeOnStack(cmd, Kilobyte(1));
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
  }

  fprintf(stdout, "[BOB] Success\n");
}
