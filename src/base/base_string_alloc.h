//
// String functions that require memory allocation
//
static S8 S8_Allocate(Arena *a, U64 size)
{
  S8 result = {};
  if (size)
  {
    result.str = Alloc(a, U8, size);
    result.size = size;
  }
  return result;
}

static S8 S8_Copy(Arena *a, S8 text)
{
  S8 result = {};
  if (text.size)
  {
    result = S8_Allocate(a, text.size);
    memcpy(result.str, text.str, result.size);
  }
  return result;
}

static char *S8_CopyToCstr(Arena *a, S8 text)
{
  char *result = Alloc(a, char, text.size + 1);
  memcpy(result, text.str, text.size);
  result[text.size] = 0;
  return result;
}

static S8 S8_ConcatArray(Arena *a, S8 *array, U64 array_count)
{
  U64 total_size = 0;
  ForU64(i, array_count)
    total_size += array[i].size;

  S8 result = S8_Allocate(a, total_size);
  U8 *dst = result.str;

  ForU64(i, array_count)
  {
    memcpy(dst, array[i].str, array[i].size);
    dst += array[i].size;
  }

  return result;
}

static S8 S8_Concat2(Arena *a, S8 t1, S8 t2)
{
  S8 array[] = {t1, t2};
  return S8_ConcatArray(a, array, ArrayCount(array));
}
static S8 S8_Concat3(Arena *a, S8 t1, S8 t2, S8 t3)
{
  S8 array[] = {t1, t2, t3};
  return S8_ConcatArray(a, array, ArrayCount(array));
}
static S8 S8_Concat4(Arena *a, S8 t1, S8 t2, S8 t3, S8 t4)
{
  S8 array[] = {t1, t2, t3, t4};
  return S8_ConcatArray(a, array, ArrayCount(array));
}
static S8 S8_Concat5(Arena *a, S8 t1, S8 t2, S8 t3, S8 t4, S8 t5)
{
  S8 array[] = {t1, t2, t3, t4, t5};
  return S8_ConcatArray(a, array, ArrayCount(array));
}

static S8 S8_FromArena(Arena *a)
{
  return (S8)
  {
    a->base,
    a->used
  };
}

static S8 S8_ToUpper(Arena *a, S8 source)
{
  S8 result = S8_Allocate(a, source.size);
  ForU64(i, source.size)
    result.str[i] = ByteToUpper(source.str[i]);
  return result;
}
static S8 S8_ToLower(Arena *a, S8 source)
{
  S8 result = S8_Allocate(a, source.size);
  ForU64(i, source.size)
    result.str[i] = ByteToLower(source.str[i]);
  return result;
}

static S8 S8_ReplaceAll(Arena *a, S8 original, S8 substring, S8 new_substring, S8_MatchFlags flags)
{
  flags &= ~S8_FindLast; // clear FindLast flag, it doesn't work here

  I64 replace_delta = (I64)new_substring.size - (I64)substring.size;
  U64 replace_count = S8_Count(original, substring, 0, flags);
  I64 total_delta = replace_delta * (I64)replace_count;

  I64 new_size = (I64)original.size + total_delta;
  S8 result = S8_Allocate(a, (U64)new_size);
  S8 res = result;
  S8 og = original;

  S8_FindResult find = S8_Find(og, substring, 0, flags);

  for (;;)
  {
    // copy data from before find, og -> res
    if (find.found && find.index > 0)
    {
      memcpy(res.str, og.str, find.index);
      res = S8_Skip(res, find.index);
      og = S8_Skip(og, find.index);
    }

    // copy reminding data and exit, og -> res
    if (!find.found)
    {
      memcpy(res.str, og.str, res.size);
      break;
    }

    // copy data at find index, new_substring -> res
    memcpy(res.str, new_substring.str, new_substring.size);
    res = S8_Skip(res, new_substring.size);
    og = S8_Skip(og, substring.size);

    // new find
    find = S8_Find(og, substring, 0, flags);
  }

  return result;
}
