//
// @todo Remove Asserts from here in the future. Instead we should be tracking errors.
//
static bool IsPointerAligned(void *ptr, U64 alignment)
{
  U64 ptr_val = (U64)ptr;
  U64 missaligned = ptr_val % alignment;
  return missaligned == 0;
}

// ---
static S8 BREAD_File()
{
  return APP.ast.bread.file;
}
static S8 BREAD_OffsetSizeToS8(U64 offset, U64 size)
{
  S8 result = S8_Substring(BREAD_File(), offset, offset+size);
  Assert(result.size == size);
  return result;
}
static S8 BREAD_FileRangeToS8(BREAD_Range range)
{
  return BREAD_OffsetSizeToS8(range.offset, range.size);
}

// ---
static void *BREAD_S8CastToPtr(S8 string, U64 check_size, U64 check_align)
{
  Assert(string.size == check_size);
  Assert(IsPointerAligned(string.str, check_align));
  return string.str;
}
#define BREAD_S8AsType(String, Type, Count) (Type *)BREAD_S8CastToPtr(String, sizeof(Type)*Count, _Alignof(Type))

static void *BREAD_FileRangeToPtr(BREAD_Range range, U64 check_type_size, U64 check_type_align)
{
  Assert(range.size == range.elem_count * check_type_size);
  S8 string = BREAD_FileRangeToS8(range);
  Assert(IsPointerAligned(string.str, check_type_align));
  return string.str;
}
#define BREAD_FileRangeAsType(Range, Type) (Type *)BREAD_FileRangeToPtr(Range, sizeof(Type), _Alignof(Type))

// ---
static void BREAD_LoadFile(const char *bread_file_path)
{
  AST_BreadFile *br = &APP.ast.bread;
  br->file = OS_LoadFile(br->arena, bread_file_path);
  Assert(br->file.size); // @todo Handle lack of file gracefully in the future

  // Header
  {
    S8 header_string = BREAD_OffsetSizeToS8(0, sizeof(BREAD_Header));
    br->header = BREAD_S8AsType(header_string, BREAD_Header, 1);
  }
  // Validate hash
  {
    S8 hashable = S8_Skip(BREAD_File(), sizeof(br->header->file_hash));
    U64 calculated_hash = S8_Hash(BREAD_MAGIC_HASH_SEED, hashable);
    Assert(br->header->file_hash == calculated_hash);
  }
  // Links
  br->links = BREAD_FileRangeAsType(br->header->links, BREAD_Links);

  // Models
  br->models_count = br->links->models.list.elem_count;
  Assert(br->models_count == MODEL_COUNT);
  br->models = BREAD_FileRangeAsType(br->links->models.list, BREAD_Model);

  // Materials
  br->materials_count = br->links->materials.elem_count;
  br->materials = BREAD_FileRangeAsType(br->links->materials, BREAD_Material);
}
