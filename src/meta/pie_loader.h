//
// @todo Remove Asserts from here in the future. Instead we should be tracking errors.
//
static bool IsPointerAligned(void *ptr, U64 alignment)
{
  U64 ptr_val = (U64)ptr;
  U64 missaligned = ptr_val % alignment;
  return missaligned == 0;
}

static S8 PIE_FileOffsetToS8(U64 offset, U64 size)
{
  S8 result = S8_Substring(PIE_File(), offset, offset+size);
  Assert(result.size == size);
  return result;
}
static S8 PIE_ListToS8(PIE_List list)
{
  // validation
  U32 type_size = TYPE_GetSize(list.type);
  U32 type_align = TYPE_GetAlign(list.type);
  Assert(list.size == list.count * type_size);

  S8 result = PIE_FileOffsetToS8(list.offset, list.size);
  Assert(IsPointerAligned(result.str, type_align)); // validation
  return result;
}

// ---
static void *PIE_S8CastToPtr(S8 string, U64 check_size, U64 check_align)
{
  Assert(string.size == check_size);
  Assert(IsPointerAligned(string.str, check_align));
  return string.str;
}
#define PIE_S8AsType(String, Type, Count) (Type *)PIE_S8CastToPtr(String, sizeof(Type)*Count, _Alignof(Type))

static void *PIE_ListToPtr(PIE_List list, TYPE_ENUM type)
{
  Assert(list.count == 0 || list.type == type); // validation
  return PIE_ListToS8(list).str;
}
#define PIE_ListAsType(List, Type) (Type *)PIE_ListToPtr(List, TYPE_##Type)
