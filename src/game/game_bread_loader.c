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
static S8 BREAD_FileOffsetToS8(U64 offset, U64 size)
{
  S8 result = S8_Substring(BREAD_File(), offset, offset+size);
  Assert(result.size == size);
  return result;
}
static S8 BREAD_ListToS8(BREAD_List list)
{
  // validation
  U32 type_size = TYPE_GetSize(list.type);
  U32 type_align = TYPE_GetAlign(list.type);
  Assert(list.size == list.count * type_size);

  S8 result = BREAD_FileOffsetToS8(list.offset, list.size);
  Assert(IsPointerAligned(result.str, type_align)); // validation
  return result;
}

// ---
static void *BREAD_S8CastToPtr(S8 string, U64 check_size, U64 check_align)
{
  Assert(string.size == check_size);
  Assert(IsPointerAligned(string.str, check_align));
  return string.str;
}
#define BREAD_S8AsType(String, Type, Count) (Type *)BREAD_S8CastToPtr(String, sizeof(Type)*Count, _Alignof(Type))

static void *BREAD_ListToPtr(BREAD_List list, TYPE_ENUM type)
{
  Assert(list.type == type); // validation
  return BREAD_ListToS8(list).str;
}
#define BREAD_ListAsType(Range, Type) (Type *)BREAD_ListToPtr(Range, TYPE_##Type)

// ---
static void BREAD_LoadFile(const char *bread_file_path)
{
  AST_BreadFile *br = &APP.ast.bread;
  br->file = OS_LoadFileLeakMemory(bread_file_path);
  Assert(br->file.size); // @todo Handle lack of file gracefully in the future

  // Header
  {
    S8 header_string = BREAD_FileOffsetToS8(0, sizeof(BREAD_Header));
    br->header = BREAD_S8AsType(header_string, BREAD_Header, 1);
  }
  // Validate hash
  {
    S8 hashable = S8_Skip(BREAD_File(), sizeof(br->header->file_hash));
    U64 calculated_hash = S8_Hash(BREAD_MAGIC_HASH_SEED, hashable);
    Assert(br->header->file_hash == calculated_hash);
  }
  // Links
  br->links = BREAD_ListAsType(br->header->links, BREAD_Links);

  // Models
  br->models_count = br->links->models.list.count;
  Assert(br->models_count == MODEL_COUNT);
  br->models = BREAD_ListAsType(br->links->models.list, BREAD_Model);

  // Materials
  br->materials_count = br->links->materials.count;
  br->materials = BREAD_ListAsType(br->links->materials, BREAD_Material);
}
