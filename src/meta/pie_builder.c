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

static void PIE_Aling(Printer *p, U64 alignment)
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

//
// Allocating bytes in printer & tracking the range
//
static void PIE_ListStart(Printer *printer_owner, PIE_List *list, TYPE_ENUM type)
{
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
  bb.rigid_vertices   = Pr_Alloc(a, small_size);
  bb.skinned_vertices = Pr_Alloc(a, small_size);
  bb.indices          = Pr_Alloc(a, small_size);

  U32 tiny_size = Kilobyte(16);
  bb.skeletons = Pr_Alloc(a, tiny_size);
  bb.materials = Pr_Alloc(a, tiny_size);

  Pr_ReserveBytes(&bb.file, sizeof(PIE_Header)); // reserve space for header
  bb.selected_model = MODEL_COUNT;
  return bb;
}

static void PIE_FinalizeBuilder()
{
  PIE_Builder *bb = &BAKER.bb;

  //
  // Prepare PIE_Links and append it to memory
  //
  PIE_Links links = {};

  PIE_ListStart(&bb->file, &links.models.rigid_vertices, TYPE_WORLD_VertexRigid);
  Pr_Printer(&bb->file, &bb->rigid_vertices);
  PIE_ListEnd(&bb->file, &links.models.rigid_vertices);

  PIE_ListStart(&bb->file, &links.models.skinned_vertices, TYPE_WORLD_VertexSkinned);
  Pr_Printer(&bb->file, &bb->skinned_vertices);
  PIE_ListEnd(&bb->file, &links.models.skinned_vertices);

  PIE_ListStart(&bb->file, &links.models.indices, TYPE_U16);
  Pr_Printer(&bb->file, &bb->indices);
  PIE_ListEnd(&bb->file, &links.models.indices);

  ForArray(model_index, bb->models)
  {
    PIE_Model *model = bb->models + model_index;
    PIE_Geometry *geomeries = PIE_ListAsType(model->geometries, PIE_Geometry);
    ForU32(geo_index, model->geometries.count)
    {
      PIE_Geometry *geo = geomeries + geo_index;
      if (!geo->indices_count)
      {
        M_LOG(M_LogGltfWarning, "PIE warning: Model %u (%s) "
              "contains uninitialized geometry (%u/%u)",
              (U32)model_index, MODEL_GetCstrName(model_index),
              geo_index, model->geometries.count);
      }
    }
  }

  // Models
  PIE_ListStart(&bb->file, &links.models.list, TYPE_PIE_Model);
  {
    PIE_Model *dst = PIE_ReserveBytes(&bb->file, sizeof(bb->models), _Alignof(PIE_Model));
    Memcpy(dst, bb->models, sizeof(bb->models));
  }
  PIE_ListEnd(&bb->file, &links.models.list);

  // Skeletons
  PIE_ListStart(&bb->file, &links.skeletons, TYPE_PIE_Skeleton);
  Pr_Printer(&bb->file, &bb->skeletons);
  PIE_ListEnd(&bb->file, &links.skeletons);

  // Materials
  PIE_ListStart(&bb->file, &links.materials, TYPE_PIE_Material);
  Pr_Printer(&bb->file, &bb->materials);
  PIE_ListEnd(&bb->file, &links.materials);

  //
  // Prepare PIE_Header
  //
  PIE_Header *header = (PIE_Header *)bb->file.buf;
  *PIE_ListReserve(&bb->file, &header->links, PIE_Links, 1) = links;

  //
  // Calculate hash
  //
  S8 whole_file = Pr_AsS8(&bb->file);
  S8 hashable_file = S8_Skip(whole_file, sizeof(header->file_hash));
  header->file_hash = S8_Hash(PIE_MAGIC_HASH_SEED, hashable_file);

  bb->finalized = true;
}

static void PIE_SaveToFile(const char *file_path)
{
  PIE_Builder *bb = &BAKER.bb;
  M_Check(bb->finalized);
  S8 content = Pr_AsS8(&bb->file);
  M_SaveFile(file_path, content);
}

//
// Helpers
//
static void PIE_AddModel(PIE_Builder *bb, MODEL_Type model_type, bool is_skinned, U32 geometry_count)
{
  M_Check(!bb->finalized);
  M_Check(model_type < MODEL_COUNT);
  bb->selected_model = model_type;

  PIE_Model *model = bb->models + bb->selected_model;
  model->is_skinned = is_skinned;

  M_Check(!model->geometries.type); // Check that model wasn't initialized already
  bb->geos = PIE_ListReserve(&bb->file, &model->geometries, PIE_Geometry, geometry_count);
  bb->geos_count = geometry_count;
  bb->geos_index = 0;
}

static void *PIE_AddModelVertex(PIE_Builder *bb, bool is_skinned)
{
  M_Check(!bb->finalized);
  Printer *vert_printer = is_skinned ? &bb->skinned_vertices : &bb->rigid_vertices;
  U64 vert_align = is_skinned ? _Alignof(WORLD_VertexSkinned) : _Alignof(WORLD_VertexRigid);
  U64 vert_size = is_skinned ? sizeof(WORLD_VertexSkinned) : sizeof(WORLD_VertexRigid);

  PIE_Geometry *geo = bb->geos + bb->geos_index;
  if (!geo->is_init_vertices) // init vertices offset
  {
    geo->is_init_vertices = true;
    geo->vertices_start_index = vert_printer->used / vert_size;
  }

  void *result = PIE_ReserveBytes(vert_printer, vert_size, vert_align);
  Memclear(result, vert_size);
  return result;
}
static WORLD_VertexSkinned *PIE_AddModelSkinnedVertex(PIE_Builder *bb) { return PIE_AddModelVertex(bb, true); }
static WORLD_VertexRigid   *PIE_AddModelRigidVertex(PIE_Builder *bb)   { return PIE_AddModelVertex(bb, false); }

static void PIE_CopyIndices(PIE_Builder *bb, U16 *src_indices, U32 src_indices_count)
{
  M_Check(!bb->finalized);
  PIE_Geometry *geo = bb->geos + bb->geos_index;

  if (!geo->is_init_indices)
  {
    geo->is_init_indices = true;
    geo->indices_start_index = bb->indices.used / sizeof(U16);
  }
  geo->indices_count += src_indices_count;

  U16 *dst = PIE_Reserve(&bb->indices, U16, src_indices_count);
  Memcpy(dst, src_indices, sizeof(U16)*src_indices_count);
}
