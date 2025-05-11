//
// @todo Remove Asserts from here in the future. Instead we should be tracking errors.
//
static S8 PIE_File()
{
  return APP.ast.pie.file;
}
#include "pie_loader.h"

static void PIE_LoadFile(const char *pie_file_path)
{
  ASSET_PieFile *br = &APP.ast.pie;
  br->file = OS_LoadFileLeakMemory(pie_file_path);
  Assert(br->file.size); // @todo Handle lack of file gracefully in the future

  // Header
  {
    S8 header_string = PIE_FileOffsetToS8(0, sizeof(PIE_Header));
    br->header = PIE_S8AsType(header_string, PIE_Header, 1);
  }
  // Validate hash
  {
    S8 hashable = S8_Skip(PIE_File(), sizeof(br->header->file_hash));
    U64 calculated_hash = S8_Hash(PIE_MAGIC_HASH_SEED, hashable);
    Assert(br->header->file_hash == calculated_hash);
  }
  // Links
  br->links = PIE_ListAsType(br->header->links, PIE_Links);

  // Models
  br->models_count = br->links->models.list.count;
  Assert(br->models_count == MODEL_COUNT);
  br->models = PIE_ListAsType(br->links->models.list, PIE_Model);

  // Materials
  br->materials_count = br->links->materials.count;
  br->materials = PIE_ListAsType(br->links->materials, PIE_Material);
}
