//
// @todo Remove Asserts from here in the future. Instead we should be tracking errors.
//
static S8 BREAD_File()
{
  return APP.ast.bread.file;
}
#include "bread_loader.h"

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
