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

static S8 BREAD_FileRange(BREAD_Range range)
{
  return BREAD_FileOffset(range.offset, range.size);
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
    bread->models_count = bread->contents->models.list.elem_count;
    Assert(bread->models_count == MODEL_COUNT);

    S8 model_array_string = BREAD_FileRange(bread->contents->models.list);
    bread->models = BREAD_AsType(model_array_string, BREAD_Model, bread->models_count);
  }
}

//
// Post init
//
static void BREAD_LoadModels()
{
  AST_BreadFile *bread = &APP.ast.bread;

  APP.ast.rigid_vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                            bread->contents->models.rigid_vertices.size,
                                            "Rigid model vertices");
  APP.ast.skinned_vertices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX,
                                              bread->contents->models.skinned_vertices.size,
                                              "Skinned model vertices");
  APP.ast.indices = GPU_CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX,
                                     bread->contents->models.indices.size,
                                     "Model indices");

  S8 rigid_string = BREAD_FileRange(bread->contents->models.rigid_vertices);
  S8 skinned_string = BREAD_FileRange(bread->contents->models.skinned_vertices);
  S8 indices_string = BREAD_FileRange(bread->contents->models.indices);

  GPU_TransferBuffer(APP.ast.rigid_vertices, rigid_string.str, rigid_string.size);
  GPU_TransferBuffer(APP.ast.skinned_vertices, skinned_string.str, skinned_string.size);
  GPU_TransferBuffer(APP.ast.indices, indices_string.str, indices_string.size);

  ForU32(model_kind, MODEL_COUNT)
  {
    BREAD_Model *bread_model = bread->models + model_kind;
    Asset *asset = APP.ast.geo_assets + model_kind;

    asset->loaded = true;
    asset->Geo.is_skinned = bread_model->is_skinned;
    asset->Geo.vertices_start_index = bread_model->vertices_start_index;
    asset->Geo.indices_start_index = bread_model->indices_start_index;
    asset->Geo.indices_count = bread_model->indices.elem_count;
  }
}
