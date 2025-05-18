//
// @todo Remove Asserts from here in the future. Instead we should be tracking errors.
//
static S8 PIE_File()
{
  return APP.ast.pie.file;
}
#include "pie_loader.h"

static void ASSET_LoadPieFile(const char *pie_file_path)
{
  ASSET_PieFile *pie = &APP.ast.pie;
  pie->file = OS_LoadFileLeakMemory(pie_file_path);
  Assert(pie->file.size); // @todo Handle lack of file gracefully in the future

  // Header
  {
    S8 header_string = PIE_FileOffsetToS8(0, sizeof(PIE_Header));
    pie->header = PIE_S8AsType(header_string, PIE_Header, 1);
  }
  // Validate hash
  {
    S8 hashable = S8_Skip(PIE_File(), sizeof(pie->header->file_hash));
    U64 calculated_hash = S8_Hash(PIE_MAGIC_HASH_SEED, hashable);
    Assert(pie->header->file_hash == calculated_hash);
  }
  // Links
  pie->links = PIE_ListAsType(pie->header->links, PIE_Links);

  // Models
  pie->models_count = pie->links->models.list.count;
  pie->models = PIE_ListAsType(pie->links->models.list, PIE_Model);

  // Materials
  pie->materials_count = pie->links->materials.count;
  pie->materials = PIE_ListAsType(pie->links->materials, PIE_Material);
}
