//
// @todo Remove Asserts from here in the future. Instead we should be tracking errors.
//

static bool IsPointerAligned(void *ptr, U64 alignment)
{
  U64 ptr_val = (U64)ptr;
  U64 align_mask = alignment - 1;
  U64 masked_off = (ptr_val & (~align_mask));
  return masked_off == ptr_val;
}

static S8 BREAD_File()
{
  return APP.ast.bread.file;
}

static S8 BREAD_FileOffset(U64 offset, U64 size)
{
  S8 result = S8_Substring(BREAD_File(), offset, offset+size);
  Assert(result.size == size);
  return result;
}

static S8 BREAD_FileRange(BREAD_Range range, U64 type_size)
{
  return BREAD_FileOffset(range.offset, range.count * type_size);
}

static void *BREAD_CastToPtr(S8 string, U64 check_size, U64 check_align)
{
  Assert(string.size == check_size);
  Assert(IsPointerAligned(string.str, check_align));
  return string.str;
}
#define BREAD_AsType(String, Type, Count) (Type *)BREAD_CastToPtr(String, sizeof(Type)*Count, _Alignof(Type))

static void BREAD_InitFile()
{
  AST_BreadFile *bread = &APP.ast.bread;
  // Header
  {
    S8 header_string = BREAD_FileOffset(0, sizeof(BREAD_Header));
    bread->header = BREAD_AsType(header_string, BREAD_Header, 1);
  }
  // Validate hash
  {
    S8 hashable = S8_Skip(BREAD_File(), sizeof(bread->header->file_hash));
    U64 calculated_hash = S8_Hash(BREAD_MAGIC_HASH_SEED, hashable);
    Assert(bread->header->file_hash == calculated_hash);
  }
  // Contents
  {
    S8 contents_string = BREAD_FileOffset(bread->header->contents_offset, sizeof(BREAD_Contents));
    bread->contents = BREAD_AsType(contents_string, BREAD_Contents, 1);
  }
  // Models
  {
    bread->models_count = bread->contents->models.count;
    Assert(bread->models_count == MDL_COUNT);

    S8 model_array_string = BREAD_FileRange(bread->contents->models, sizeof(BREAD_Model));
    bread->models = BREAD_AsType(model_array_string, BREAD_Model, bread->models_count);
  }
}

//
// Post init
//
static void BREAD_LoadModel(MDL_Kind model_kind)
{
  Assert(model_kind < MDL_COUNT);
  AST_BreadFile *bread = &APP.ast.bread;

  BREAD_Model *bread_model = bread->models + model_kind;
  Asset *asset = APP.ast.geo_assets + model_kind;

  U64 vert_size = (bread_model->is_skinned ? sizeof(MDL_GpuSkinnedVertex) : sizeof(MDL_GpuRigidVertex));
  U64 vert_align = (bread_model->is_skinned ? _Alignof(MDL_GpuSkinnedVertex) : _Alignof(MDL_GpuRigidVertex));

  S8 vertices_string = BREAD_FileRange(bread_model->vertices, vert_size);
  S8 indices_string = BREAD_FileRange(bread_model->indices, sizeof(U16));
  // Just for checking alignmnets:
  void *vertices = BREAD_CastToPtr(vertices_string, bread_model->vertices.count*vert_size, vert_align);
  U16 *indices = BREAD_AsType(indices_string, U16, bread_model->indices.count);
  (void)vertices;
  (void)indices;

  asset->loaded = true;
  asset->Geo.vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertices_string.size, 0);
  asset->Geo.indices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, indices_string.size, 0);

  GPU_TransferBuffer(asset->Geo.vertices, vertices_string.str, vertices_string.size);
  GPU_TransferBuffer(asset->Geo.indices, indices_string.str, indices_string.size);
}
