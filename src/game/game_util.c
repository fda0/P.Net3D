//
// Queue
//
static U8 *Queue_Push(void *buf, U64 elem_count, U64 elem_size, RngU64 *range)
{
  Assert(range->min <= range->max);
  U64 current_index = range->max % elem_count;

  // truncate min if it overlaps with current
  if (range->min != range->max) // check that queue is not empty
  {
    U64 min_index = range->min % elem_count;
    if (min_index == current_index)
    {
      range->min += 1;
    }
  }

  // advance max
  range->max += 1;

  U8 *res = (U8 *)buf + (current_index * elem_size);
  return res;
}

static U8 *Queue_Peek(void *buf, U64 elem_count, U64 elem_size, RngU64 *range)
{
  Assert(range->min <= range->max);
  if (range->min >= range->max)
    return 0;

  U64 current_index = range->min % elem_count;
  U8 *res = (U8 *)buf + (current_index * elem_size);
  return res;
}

static U8 *Queue_Pop(void *buf, U64 elem_count, U64 elem_size, RngU64 *range)
{
  U8 *res = Queue_Peek(buf, elem_count, elem_size, range);

  // advance min
  if (res)
    range->min += 1;

  return res;
}

static U8 *Queue_PeekAt(void *buf, U64 elem_count, U64 elem_size, RngU64 *range, U64 peek_index)
{
  Assert(range->min <= range->max);
  if (range->min >= range->max)
    return 0;

  if (!(peek_index >= range->min && peek_index < range->max))
    return 0;

  U64 index = peek_index % elem_count;
  U8 *res = (U8 *)buf + (index * elem_size);
  return res;
}

// These macros used (typeof(&*Arr)) in the past - but that requires C23 spec. At the same time MSVC std:clastest target crashes when I try to use clib library...
#define Q_Push(Arr, RangePtr) Queue_Push(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr)
#define Q_Pop(Arr, RangePtr) Queue_Pop(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr)
#define Q_Peek(Arr, RangePtr) Queue_Peek(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr)
#define Q_PeekAt(Arr, RangePtr, Index) Queue_PeekAt(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr, Index)

//
// clamps
//
static U16 Saturate_U64toU16(U64 number)
{
  if (number > SDL_MAX_UINT16)
    return SDL_MAX_UINT16;
  return number & 0xffff;
}
static U16 Checked_U64toU16(U64 number)
{
  Assert(number <= SDL_MAX_UINT16);
  return number & 0xffff;
}

static U32 Checked_U64toU32(U64 number)
{
  Assert(number <= SDL_MAX_UINT32);
  return number & 0xffffffff;
}

//
// TickDeltas
//
static bool TickDeltas_AddTick(TickDeltas *td, U64 tick)
{
  if (td->last_tick == tick)
    return false;

  bool first_init = !td->last_tick;
  td->last_tick = tick;

  if (first_init)
    return false;

  U64 delta_u64 = (td->last_tick > tick ? 0 : tick - td->last_tick);
  U16 delta = Saturate_U64toU16(delta_u64);

  AssertBounds(td->next_delta_index, td->deltas);
  U64 index = td->next_delta_index;
  td->next_delta_index = (td->next_delta_index + 1) % ArrayCount(td->deltas);

  td->deltas[index] = delta;
  return true;
}

static void TickDeltas_UpdateCatchup(TickDeltas *td, U16 current_delta)
{
  if (!td->last_tick)
    return;

  U16 max_delta = 0;
  ForArray(i, td->deltas)
  {
    if (td->deltas[i] > max_delta)
      max_delta = td->deltas[i];
  }

  U16 target_delta = max_delta + (max_delta / 16) + 4;
  if (current_delta > target_delta)
  {
    td->tick_catchup = current_delta - target_delta;
  }
  else
  {
    td->tick_catchup = 0;
  }
}

//
// Time benchmarking
//
static U64 TI_PerfCounter()
{
  return SDL_GetPerformanceCounter();
}

static void TI_LogPerfArray(const char *label, U64 *arr, U64 count)
{
  if (count > 1)
  {
    LOG(LOG_Perf, "Perf counters (%llu) for %s:", count, label);
    ForU64(i, count - 1)
    {
      U64 delta_u64 = arr[i + 1] - arr[i];
      double delta_d = (double)delta_u64*1000.0 / (double)SDL_GetPerformanceFrequency();
      LOG(LOG_Perf, "%fms", delta_d);
    }
  }
  else
  {
    LOG(LOG_Perf, "Perf counters - only %llu measurement for %s", count, label);
  }
}

//
//
//
static U32 CalculateMipMapCount(U32 width, U32 height)
{
  U32 max_dim = Max(width, height);
  I32 msb = MostSignificantBitU32(max_dim);
  if (msb < 0) return 0;
  return msb;
}

//
//
//
static S8 OS_LoadFileLeakMemory(const char *file_path)
{
  U64 size = 0;
  U8 *data = SDL_LoadFile(file_path, &size);
  return S8_Make(data, size);
}

static S8 OS_LoadFile(Arena *a, const char *file_path)
{
  // @todo Going through malloc memory and copying it into arena is obviously stupid
  //       Improve in the future.
  U64 size = 0;
  U8 *data = SDL_LoadFile(file_path, &size); // go through better api
  if (!data)
    LOG(LOG_OS, "Failed to load file %s", file_path);

  U8 *dest = Arena_AllocateBytes(a, size, 64, false); // high alignment
  Memcpy(dest, data, size);
  S8 result = S8_Make(dest, size);

  SDL_free(data); // go through better api
  return result;
}

static void OS_SaveFile(const char *file_path, S8 data)
{
  bool success = SDL_SaveFile(file_path, data.str, data.size);
  if (!success)
    LOG(LOG_OS, "Failed to save to file %s", file_path);
}

//
// Only bottom 48 bits of pointers are actually used on current x64 CPUs
//
static void *PackDataIntoPointer(void *pointer, U16 data)
{
  return (void *)((U64)pointer | ((U64)data << 48));
}
static U16 PointerDecodeData(void *pointer)
{
  return (U16)((U64)pointer >> 48);
}
static void *PointerMakeCanonical(void *pointer)
{
  return (void *)((U64)pointer & 0xffffffffffffull);
}

//
//
//
static S8 S8_FromClaySlice(Clay_StringSlice css)
{
  return S8_Make((U8 *)css.chars, css.length);
}

static Clay_String ClayString_FromS8(S8 s)
{
  Clay_String result =
  {
    .length = s.size,
    .chars = (char *)s.str
  };
  return result;
}

static U32 Color32_ClayColor(Clay_Color color)
{
  return Color32_RGBAi((U32)color.r, (U32)color.g, (U32)color.b, (U32)color.a);
}
