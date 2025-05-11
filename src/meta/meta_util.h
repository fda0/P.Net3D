#define _CRT_SECURE_NO_WARNINGS
#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>
#include "stdio.h"
#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"
#include "base_string_alloc.h"

enum M_LogType
{
  M_Idk         = (1 << 0),
  M_Err         = (1 << 1),
  M_GLTFWarning = (1 << 3),
  M_GLTFDebug   = (1 << 4),
  M_TEXWarning  = (1 << 5),
};

static struct
{
  U32 reject_filter;
  I32 error_count;
} M_LogState;

static bool M_LogFlagCheck(I32 flags)
{
  if (flags & M_Err)
    M_LogState.error_count += 1;
  return !(M_LogState.reject_filter & flags);
}

#ifndef M_LOG_HEADER
#define M_LOG_HEADER "[META] "
#endif

// logging enable/disable
#if 1
#define M_LOG(FLAGS, ...) do{ if(M_LogFlagCheck(FLAGS)){ SDL_Log(M_LOG_HEADER __VA_ARGS__); }}while(0)
#else
#define M_LOG(FLAGS, ...) do{ if(M_LogFlagCheck(FLAGS) && 0){ printf(M_LOG_HEADER __VA_ARGS__); }}while(0)
#endif

#define M_Check(...) SDL_assert(__VA_ARGS__)

static S8 M_LoadFile(const char *file_path, bool exit_on_err)
{
  U64 size = 0;
  void *data = SDL_LoadFile(file_path, &size);
  if (exit_on_err && !data)
  {
    M_LOG(M_Err, "Failed to load file %s", file_path);
    exit(1);
  }

  S8 result =
  {
    (U8 *)data,
    size
  };
  return result;
}

static void M_SaveFile(const char *file_path, S8 data)
{
  bool success = SDL_SaveFile(file_path, data.str, data.size);
  if (!success)
  {
    M_LOG(M_Err, "Failed to save to file %s", file_path);
    exit(1);
  }
}

static S8 M_NameFromPath(S8 path)
{
  S8 model_name = path;
  {
    S8_FindResult slash = S8_Find(model_name, S8Lit("/"), 0, S8_FindLast|S8_SlashInsensitive);
    if (slash.found)
    {
      model_name = S8_Skip(model_name, slash.index + 1);
    }
    S8_FindResult dot = S8_Find(model_name, S8Lit("."), 0, 0);
    if (dot.found)
    {
      model_name = S8_Prefix(model_name, dot.index);
    }
  }
  return model_name;
}
