//
// Allocating bytes in printer
//
static void *PIE_ReserveBytes(Printer *p, U64 size, U64 alignment)
{
  M_Check(p->used % alignment == 0);
  void *pie_reserve_result = Pr_ReserveBytes(p, size);
  M_Check(pie_reserve_result);
  M_Check(!p->err);
  Memclear(pie_reserve_result, size);
  return pie_reserve_result;
}
#define PIE_Reserve(P, Type, Count) (Type *)PIE_ReserveBytes(P, sizeof(Type)*(Count), _Alignof(Type))

static void PIE_Align(Printer *p, U64 alignment)
{
  U64 missaligned_bytes = p->used % alignment;
  if (missaligned_bytes)
  {
    U64 to_align = alignment - missaligned_bytes;
    void *data = Pr_ReserveBytes(p, to_align);
    M_Check(data);
    Memclear(data, to_align);
  }
}

static void PIE_AlignType(Printer *p, TYPE_ENUM type)
{
  PIE_Align(p, TYPE_GetAlign(type));
}

//
// Allocating bytes in printer & tracking the range
//
static void PIE_ListStart(Printer *printer_owner, PIE_List *list, TYPE_ENUM type)
{
  M_Check(!list->type);
  PIE_AlignType(printer_owner, type);
  list->type = type;
  list->offset = printer_owner->used;
}
static void PIE_ListEnd(Printer *printer_owner, PIE_List *list)
{
  M_Check(list->type && !list->count && !list->size);

  M_Check(printer_owner->used >= list->offset);
  list->size = printer_owner->used - list->offset;

  U32 type_size = TYPE_GetSize(list->type);
  M_Check(list->size % type_size == 0);
  list->count = list->size / type_size;
}
static void *PIE_ListReserveBytes(Printer *p, PIE_List *list, TYPE_ENUM type, U32 count)
{
  PIE_ListStart(p, list, type);
  void *result = PIE_ReserveBytes(p, TYPE_GetSize(type)*count, TYPE_GetAlign(type));
  PIE_ListEnd(p, list);
  return result;
}
#define PIE_ListReserve(P, List, Type, Count) (Type *)PIE_ListReserveBytes(P, List, TYPE_##Type, Count)

//
// Builder create and finalize
//
static PIE_Builder PIE_CreateBuilder(Arena *a, U32 max_file_size)
{
  PIE_Builder bb = {};
  bb.file = Pr_Alloc(a, max_file_size);

  U32 small_size = max_file_size / 16;
  bb.vertices = Pr_Alloc(a, small_size);
  bb.indices = Pr_Alloc(a, small_size);

  U32 tiny_size = Kilobyte(16);
  bb.skeletons = Pr_Alloc(a, tiny_size);
  bb.materials = Pr_Alloc(a, tiny_size);

  Pr_ReserveBytes(&bb.file, sizeof(PIE_Header)); // reserve space for header
  return bb;
}

static void PIE_FinalizeBuilder()
{
  PIE_Builder *pbuild = &BAKER.pie_builder;

  //
  // Prepare PIE_Links and append it to memory
  //
  PIE_Links links = {};

  PIE_ListStart(&pbuild->file, &links.models.vertices, TYPE_WORLD_Vertex);
  Pr_Printer(&pbuild->file, &pbuild->vertices);
  PIE_ListEnd(&pbuild->file, &links.models.vertices);

  PIE_ListStart(&pbuild->file, &links.models.indices, TYPE_U16);
  Pr_Printer(&pbuild->file, &pbuild->indices);
  PIE_ListEnd(&pbuild->file, &links.models.indices);

  ForArray(model_index, pbuild->models)
  {
    PIE_Model *model = pbuild->models + model_index;
    PIE_Geometry *geomeries = PIE_ListAsType(model->geometries, PIE_Geometry);
    ForU32(geo_index, model->geometries.count)
    {
      PIE_Geometry *geo = geomeries + geo_index;
      if (!geo->indices_count)
      {
        M_LOG(M_GLTFWarning, "PIE warning: Model %u (%s) "
              "contains uninitialized geometry (%u/%u)",
              (U32)model_index, MODEL_GetCstrName(model_index),
              geo_index, model->geometries.count);
      }
    }
  }

  // Models
  PIE_ListStart(&pbuild->file, &links.models.list, TYPE_PIE_Model);
  {
    PIE_Model *dst = PIE_ReserveBytes(&pbuild->file, sizeof(pbuild->models), _Alignof(PIE_Model));
    Memcpy(dst, pbuild->models, sizeof(pbuild->models));
  }
  PIE_ListEnd(&pbuild->file, &links.models.list);

  // Skeletons
  PIE_ListStart(&pbuild->file, &links.skeletons, TYPE_PIE_Skeleton);
  Pr_Printer(&pbuild->file, &pbuild->skeletons);
  PIE_ListEnd(&pbuild->file, &links.skeletons);

  // Materials
  PIE_ListStart(&pbuild->file, &links.materials, TYPE_PIE_Material);
  Pr_Printer(&pbuild->file, &pbuild->materials);
  PIE_ListEnd(&pbuild->file, &links.materials);

  //
  // Prepare PIE_Header
  //
  PIE_Header *header = (PIE_Header *)pbuild->file.buf;
  *PIE_ListReserve(&pbuild->file, &header->links, PIE_Links, 1) = links;

  //
  // Calculate hash
  //
  S8 whole_file = Pr_AsS8(&pbuild->file);
  S8 hashable_file = S8_Skip(whole_file, sizeof(header->file_hash));
  header->file_hash = S8_Hash(PIE_MAGIC_HASH_SEED, hashable_file);

  pbuild->finalized = true;
}

static void PIE_SaveToFile(const char *file_path)
{
  PIE_Builder *build = &BAKER.pie_builder;
  M_Check(build->finalized);
  S8 content = Pr_AsS8(&build->file);
  M_SaveFile(file_path, content);
}
