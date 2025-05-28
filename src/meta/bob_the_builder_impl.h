#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <time.h>

#define BASE_SKIP_ARENA
#define BASE_SKIP_MATH
#include "base_types.h"
#include "base_string.h"
#include "base_hash.h"
#include "base_printer.h"
#define BOB_TMP_SIZE Kilobyte(32)
#define BOB_PrinterOnStack(p) Pr_MakeOnStack(p, BOB_TMP_SIZE)
#define BOB_out stderr
#define BOB_err stderr

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
  BOB_B3 run;
  BOB_B3 cache;
  BOB_B3 show_out;
  BOB_B3 show_in;
} BOB_Compilation;

typedef struct
{
  const char *inputs;
  const char *link_inputs;
  const char *output;
  BOB_Compilation comp;
  bool gui;
  bool sdl3;
  bool sdl3_net;
  bool sdl3_ttf;
  bool sdl3_image;
} BOB_BuildConfig;

typedef struct
{
  bool is_compilation;

  bool *check_if_cached;
  I32 cache_paths_count;
  const char **cache_paths;
  bool force_invalidate_cache;
} BOB_SectionConfig;

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
  bool show_output;
  bool show_input;
  bool did_build;
  clock_t time;
  clock_t start_time;

  U8 log_buffer[BOB_TMP_SIZE];
  Printer log;
} BOB_State;
static BOB_State BOB;

//
// Bob function implementations
//
static void BOB_CompInherit(BOB_Compilation *parent, BOB_Compilation *child)
{
  if (child->enabled == BOB_DEFAULT)     child->enabled = parent->enabled;
  if (child->compiler == BOB_CC_DEFAULT) child->compiler = parent->compiler;
  if (child->release == BOB_DEFAULT)     child->release = parent->release;
  if (child->asan == BOB_DEFAULT)        child->asan = parent->asan;
  if (child->run == BOB_DEFAULT)         child->run = parent->run;
  if (child->cache == BOB_DEFAULT)       child->cache = parent->cache;
  if (child->show_out == BOB_DEFAULT)     child->show_out = parent->show_out;
  if (child->show_in == BOB_DEFAULT)     child->show_in = parent->show_in;
}

static void BOB_Init(I32 args_count, char **args)
{
  BOB.comp.enabled = BOB_TRUE;
  BOB.start_time = clock();
  BOB.log = Pr_Make(BOB.log_buffer, sizeof(BOB.log_buffer));

  for (I32 i = 1; i < args_count; i += 1)
  {
    S8 arg = S8_FromCstr(args[i]);
    S8_MatchFlags f = S8_CaseInsensitive;

    // Options
    if      (S8_Match(arg, S8Lit("release"), f)) BOB.comp.release = BOB_TRUE;
    else if (S8_Match(arg, S8Lit("debug"), f))   BOB.comp.release = BOB_FALSE;
    else if (S8_Match(arg, S8Lit("msvc"), f))    BOB.comp.compiler = BOB_CC_MSVC;
    else if (S8_Match(arg, S8Lit("clang"), f))   BOB.comp.compiler = BOB_CC_CLANG;
    else if (S8_Match(arg, S8Lit("asan"), f))    BOB.comp.asan = BOB_TRUE;
    else if (S8_Match(arg, S8Lit("noasan"), f))  BOB.comp.asan = BOB_FALSE;
    else if (S8_Match(arg, S8Lit("run"), f))     BOB.comp.run = BOB_TRUE;
    else if (S8_Match(arg, S8Lit("norun"), f))   BOB.comp.run = BOB_FALSE;
    else if (S8_Match(arg, S8Lit("cache"), f))   BOB.comp.cache = BOB_TRUE;
    else if (S8_Match(arg, S8Lit("nocache"), f)) BOB.comp.cache = BOB_FALSE;
    else if (S8_Match(arg, S8Lit("out"), f))     BOB.comp.show_out = BOB_TRUE;
    else if (S8_Match(arg, S8Lit("noout"), f))   BOB.comp.show_out = BOB_FALSE;
    else if (S8_Match(arg, S8Lit("in"), f))      BOB.comp.show_in = BOB_TRUE;
    else if (S8_Match(arg, S8Lit("noin"), f))    BOB.comp.show_in = BOB_FALSE;

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
      fprintf(BOB_err, "[BOB] WARNING: Skipping unknown command line option: %.*s\n", S8Print(arg));
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
}

static I32 BOB_Run(const char *command)
{
  if (BOB.show_input)
    fprintf(BOB_out, "[BOB] RUNNING: %s\n", command);

  // This is what's needed to create and save output of a process in Win32
  // @todo put this in base_os.h in the future

  BOB_PrinterOnStack(cmd);
  Pr_Cstr(&cmd, "/c ");
  Pr_Cstr(&cmd, command);
  char *cmd_cstr = Pr_AsCstr(&cmd);


  SECURITY_ATTRIBUTES security =
  {
    .nLength = sizeof(security),
    .bInheritHandle = true,
  };

  HANDLE read_pipe = 0;
  HANDLE write_pipe = 0;
  bool pipe_result = CreatePipe(&read_pipe, &write_pipe, &security, 0);
  if (!pipe_result)
  {
    fprintf(BOB_err, "[BOB] CRITICAL: Failed at CreatePipe\n");
    exit(1);
  }
  if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0))
  {
    fprintf(BOB_err, "[BOB] CRITICAL: Failed at SetHandleInformation\n");
    exit(1);
  }

  STARTUPINFOA startup =
  {
    .cb = sizeof(startup),
    .dwFlags = STARTF_USESTDHANDLES,
    .hStdError = write_pipe,
    .hStdOutput = write_pipe,
  };

  PROCESS_INFORMATION process = {};

  const char *cmd_exe = "C:\\Windows\\System32\\cmd.exe";
  U32 process_result =
    CreateProcessA(cmd_exe, cmd_cstr, 0, 0, true,
                   CREATE_NO_WINDOW | CREATE_SUSPENDED,
                   0, 0/*cwd*/, &startup, &process);

  if (!process_result)
  {
    fprintf(BOB_err, "[BOB] CRITICAL: Failed at CreateProcessA\n");
    exit(1);
  }

  // Attach kill job here?
  ResumeThread(process.hThread);

  CloseHandle(write_pipe);
  CloseHandle(process.hThread);

  BOB_PrinterOnStack(cmd_output);
  for (;;)
  {
    char output_buffer[BOB_TMP_SIZE];
    U32 read_bytes = 0;
    U32 read_result = ReadFile(read_pipe, output_buffer, sizeof(output_buffer), &read_bytes, 0);
    if (!read_result || read_bytes == 0)
      break;

    S8 read_string = S8_Make(output_buffer, read_bytes);
    if (BOB.show_output)
      fprintf(BOB_out, "%.*s", S8Print(read_string));
    else
      Pr_S8(&cmd_output, read_string);
  }

  DWORD exit_code = 0;
  bool exit_result = GetExitCodeProcess(process.hProcess, &exit_code);
  if (!exit_result)
  {
    fprintf(BOB_err, "[BOB] CRITICAL: Failed at GetExitCodeProcess\n");
    exit(1);
  }

  if (exit_code)
    fprintf(BOB_out, "%s", Pr_AsCstr(&cmd_output));

  CloseHandle(read_pipe);
  CloseHandle(process.hProcess);

  return exit_code;
}
static I32 BOB_RunPrinter(Printer *p)
{
  const char *command = Pr_AsCstr(p);
  if (p->err)
  {
    fprintf(BOB_err, "[BOB] ERROR: Internal printer error\n");
    exit(1);
  }
  return BOB_Run(command);
}

static I32 BOB_RunIcon(BOB_Compilation comp, const char *icon_name)
{
  BOB_PrinterOnStack(cmd);
  Pr_Cstr(&cmd, comp.compiler == BOB_CC_CLANG ? "llvm-rc" : "rc");
  Pr_Cstr(&cmd, " /nologo /fo ");
  Pr_Cstr(&cmd, icon_name);
  Pr_Cstr(&cmd, ".res ../res/ico/");
  Pr_Cstr(&cmd, icon_name);
  Pr_Cstr(&cmd, ".rc");
  return BOB_RunPrinter(&cmd);
}

static void BOB_CheckError(I32 error_code)
{
  if (error_code)
  {
    fprintf(BOB_err, "[BOB] %s...\n", Pr_AsCstr(&BOB.log));
    fprintf(BOB_err, "[BOB] ERROR: Exiting with code %d\n", error_code);
    exit(error_code);
  }
}

static void BOB_BuildCommand(Printer *cmd, BOB_BuildConfig config)
{
  bool release = config.comp.release == BOB_TRUE;
  bool clang = config.comp.compiler == BOB_CC_CLANG;
  bool asan = config.comp.asan == BOB_TRUE;

  // Compile/Link definitions
  const char *sdl_path_debug = "build/win/Debug/";
  const char *sdl_path_release = "build/win/Release/";
  const char *include_paths =
    "-I../src/base/ -I../src/game/ -I../src/meta/ -I../gen/ -I../libs/ "
    "-I../libs/SDL/include/ -I../libs/SDL_ttf/include/ -I../libs/SDL_net/include/ -I../libs/SDL_image/include/ ";
  const char *enable_asan  = "-fsanitize=address ";

  // Compile/Link definitions per compiler
  const char *msvc_exe       = "cl ";
  const char *msvc_debug     = "/DBUILD_DEBUG=0 /Od /Ob1 /MDd ";
  const char *msvc_release   = "/DBUILD_DEBUG=1 /O2 /MD ";
  const char *msvc_common    = "/nologo /FC /Z7 /W4 /wd4244 /wd4201 /wd4324 /std:c17 ";
  const char *msvc_link      = "/link /MANIFEST:EMBED /INCREMENTAL:NO /pdbaltpath:%_PDB% ";
  const char *msvc_link_libs = "User32.lib Advapi32.lib Shell32.lib Gdi32.lib Version.lib OleAut32.lib Imm32.lib Ole32.lib Cfgmgr32.lib Setupapi.lib Winmm.lib Ws2_32.lib Iphlpapi.lib ";
  const char *msvc_link_gui  = "/SUBSYSTEM:WINDOWS ";
  const char *msvc_out       = "/out:";

  const char *clang_exe       = "clang ";
  const char *clang_debug     = "-DBUILD_DEBUG=0 -g -O0 ";
  const char *clang_release   = "-DBUILD_DEBUG=1 -g -O2 ";
  const char *clang_common    = "-fdiagnostics-absolute-paths -Wall -Werror -Wno-missing-braces -Wno-unused-function -Wno-microsoft-static-assert -Wno-c2x-extensions ";
  const char *clang_link      = "-fuse-ld=lld -Xlinker /MANIFEST:EMBED -Xlinker /pdbaltpath:%_PDB% ";
  const char *clang_link_libs = "-lUser32.lib -lAdvapi32.lib -lShell32.lib -lGdi32.lib -lVersion.lib -lOleAut32.lib -lImm32.lib -lOle32.lib -lCfgmgr32.lib -lSetupapi.lib -lWinmm.lib -lWs2_32.lib -lIphlpapi.lib ";
  const char *clang_link_gui  = "-Xlinker /SUBSYSTEM:WINDOWS ";
  const char *clang_out       = "-o ";

  // Select compiler
  const char *exe = clang ? clang_exe : msvc_exe;
  const char *opt_debug = clang ? clang_debug : msvc_debug;
  const char *opt_release = clang ? clang_release : msvc_release;
  const char *common = clang ? clang_common : msvc_common;
  const char *link = clang ? clang_link : msvc_link;
  const char *link_libs = clang ? clang_link_libs : msvc_link_libs;
  const char *link_gui = clang ? clang_link_gui : msvc_link_gui;
  const char *out = clang ? clang_out : msvc_out;

  // Select optimizations
  const char *opt_mode = release ? opt_release : opt_debug;
  const char *sdl_path = release ? sdl_path_release : sdl_path_debug;

  // Build command string
  Pr_Cstr(cmd, exe);
  Pr_Cstr(cmd, config.inputs);
  Pr_Cstr(cmd, " ");
  Pr_Cstr(cmd, opt_mode);
  Pr_Cstr(cmd, common);
  Pr_Cstr(cmd, include_paths);

  // @todo Inject git hash predefine
  // for /f %%i in ('call git describe --always --dirty') do set compile=%compile% -DBUILD_GIT_HASH=\"%%i\"

  if (asan) Pr_Cstr(cmd, enable_asan);

  Pr_Cstr(cmd, link);
  if (config.link_inputs)
  {
    Pr_Cstr(cmd, " ");
    Pr_Cstr(cmd, config.link_inputs);
    Pr_Cstr(cmd, " ");
  }

  if (config.gui) Pr_Cstr(cmd, link_gui);
  Pr_Cstr(cmd, link_libs);

  if (config.sdl3)
  {
    Pr_Cstr(cmd, "../libs/SDL/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3-static.lib ");
  }
  if (config.sdl3_net)
  {
    Pr_Cstr(cmd, "../libs/SDL_net/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3_net-static.lib ");
  }
  if (config.sdl3_ttf)
  {
    Pr_Cstr(cmd, "../libs/SDL_ttf/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3_ttf-static.lib ");
  }
  if (config.sdl3_image)
  {
    Pr_Cstr(cmd, "../libs/SDL_image/");
    Pr_Cstr(cmd, sdl_path);
    Pr_Cstr(cmd, "SDL3_image-static.lib ");
  }

  if (config.output)
  {
    Pr_Cstr(cmd, out);
    Pr_Cstr(cmd, config.output);
  }
}

static void BOB_ModTimePathsHashInner(HashState *hash_state, I32 paths_count, const char **paths)
{
  ForI32(path_index, paths_count)
  {
    // Prepare wildcard path
    BOB_PrinterOnStack(p);
    Pr_Cstr(&p, paths[path_index]);

    if (!S8_EndsWith(Pr_AsS8(&p), S8Lit("*"), 0))
    {
      if (!S8_EndsWith(Pr_AsS8(&p), S8Lit("/"), S8_SlashInsensitive))
        Pr_Cstr(&p, "/");

      Pr_Cstr(&p, "*");
    }

    WIN32_FIND_DATAA data = {};
    HANDLE handle = FindFirstFileA(Pr_AsCstr(&p), &data);
    if (handle != INVALID_HANDLE_VALUE)
    {
      do
      {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          // Is directory
          S8 dir_name = S8_FromCstr(data.cFileName);
          if (!S8_StartsWith(dir_name, S8Lit("."), 0)) // skips ".", ".." and directories starting with "."
          {
            S8_HashAbsorb(hash_state, dir_name);
            U64_HashAbsorb(hash_state, &data.ftLastWriteTime, sizeof(data.ftLastWriteTime));

            Pr_Reset(&p);
            Pr_S8(&p, dir_name);
            Pr_Cstr(&p, "/*");
            const char *wildcard_dir = Pr_AsCstr(&p);
            BOB_ModTimePathsHashInner(hash_state, 1, &wildcard_dir);
          }
        }
        else
        {
          // Is file
          S8 file_name = S8_FromCstr(data.cFileName);
          S8_HashAbsorb(hash_state, file_name);
          U64_HashAbsorb(hash_state, &data.ftLastWriteTime, sizeof(data.ftLastWriteTime));
        }
      } while (FindNextFileA(handle, &data) != 0);

      FindClose(handle);
    }
  }
}

static U64 BOB_ModTimePathsHash(I32 paths_count, const char **paths)
{
  HashState hs = HashBegin();
  BOB_ModTimePathsHashInner(&hs, paths_count, paths);
  return HashEnd(&hs);
}

static U64 BOB_LoadCacheHash(const char *file_name)
{
  BOB_PrinterOnStack(path_p);
  Pr_Cstr(&path_p, file_name);
  Pr_Cstr(&path_p, ".bob_hash");
  char *file_path = Pr_AsCstr(&path_p);

  U64 result = 0;
  U32 read_bytes = 0;

  U32 share_flags = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
  HANDLE handle = CreateFileA(file_path, GENERIC_READ, share_flags, 0, OPEN_EXISTING, 0, 0);
  if (handle != INVALID_HANDLE_VALUE)
  {
    U32 success = ReadFile(handle, &result, sizeof(result), &read_bytes, 0);
    if (!success || sizeof(result) != read_bytes)
      result = 0;

    CloseHandle(handle);
  }

  return result;
}

static void BOB_SaveCacheHash(const char *file_name, U64 cache_value)
{
  BOB_PrinterOnStack(path_p);
  Pr_Cstr(&path_p, file_name);
  Pr_Cstr(&path_p, ".bob_hash");
  char *file_path = Pr_AsCstr(&path_p);

  U32 share_flags = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
  HANDLE handle = CreateFileA(file_path, GENERIC_WRITE, share_flags, 0, CREATE_ALWAYS, 0, 0);
  if (handle != INVALID_HANDLE_VALUE)
  {
    WriteFile(handle, &cache_value, sizeof(cache_value), 0, 0);
    CloseHandle(handle);
  }
}

static void BOB_MarkSection(const char *name, BOB_Compilation comp, BOB_SectionConfig config)
{
  BOB.show_output = comp.show_out == BOB_TRUE;
  BOB.show_input = comp.show_in == BOB_TRUE;

  clock_t new_time = clock();
  if (BOB.did_build)
  {
    float delta_seconds = (float)(new_time - BOB.time) / CLOCKS_PER_SEC;
    if (delta_seconds < 0.0001f)
    {
      Pr_Cstr(&BOB.log, "; ");
    }
    else
    {
      Pr_Cstr(&BOB.log, " ");
      Pr_Float3(&BOB.log, delta_seconds);
      Pr_Cstr(&BOB.log, "s; ");
    }
  }

  BOB.did_build = true;
  BOB.time = new_time;

  if (name)
  {
    Pr_Cstr(&BOB.log, name);
  }

  if (config.check_if_cached)
  {
    U64 from_file_hash = 0;
    if (comp.cache != BOB_FALSE)
      from_file_hash = BOB_LoadCacheHash(name);

    U64 current_hash = 0;
    if (!config.force_invalidate_cache)
      current_hash = BOB_ModTimePathsHash(config.cache_paths_count, config.cache_paths);
    BOB_SaveCacheHash(name, current_hash);

    bool is_cached = (from_file_hash && from_file_hash == current_hash);
    *config.check_if_cached = is_cached;

    Pr_Cstr(&BOB.log, is_cached ? ".C" : ".UC");

    if (BOB.show_output || BOB.show_input)
    {
      Pr_Cstr(&BOB.log, "(");
      Pr_U32Hex(&BOB.log, from_file_hash);
      if (!is_cached)
      {
        Pr_Cstr(&BOB.log, " -> ");
        Pr_U32Hex(&BOB.log, current_hash);
      }
      Pr_Cstr(&BOB.log, ")");
    }
  }

  if (config.is_compilation)
  {
    if (comp.compiler == BOB_CC_CLANG) Pr_Cstr(&BOB.log, ".ll");
    if (comp.release == BOB_TRUE) Pr_Cstr(&BOB.log, ".R");
    if (comp.asan == BOB_TRUE) Pr_Cstr(&BOB.log, ".asan");
  }

  if (!name)
  {
    float delta_seconds = (float)(new_time - BOB.start_time) / CLOCKS_PER_SEC;
    Pr_Cstr(&BOB.log, "Total ");
    Pr_Float3(&BOB.log, delta_seconds);
    Pr_Cstr(&BOB.log, "s; ");
  }
}

static bool BOB_FileExists(const char *file_path)
{
  bool result = 0;

  WIN32_FIND_DATAA data = {};
  HANDLE handle = FindFirstFileA(file_path, &data);
  if (handle != INVALID_HANDLE_VALUE)
  {
    result = true;
    FindClose(handle);
  }

  return result;
}
