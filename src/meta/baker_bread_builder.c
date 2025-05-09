//
// Allocating bytes in printer
//
static void *BREAD_ReserveBytes(Printer *p, U64 size, U64 alignment)
{
  M_Check(p->used % alignment == 0);
  void *result = Pr_ReserveBytes(p, size);
  M_Check(result);
  M_Check(!p->err);
  Memclear(result, size);
  return result;
}
#define BREAD_Reserve(P, Type, Count) (Type *)BREAD_ReserveBytes(P, sizeof(Type)*(Count), _Alignof(Type))

static void BREAD_Aling(Printer *p, U64 alignment)
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
static void BREAD_ListStart(Printer *printer_owner, BREAD_List *list, TYPE_ENUM type)
{
  list->type = type;
  list->offset = printer_owner->used;
}
static void BREAD_ListEnd(Printer *printer_owner, BREAD_List *list)
{
  M_Check(list->type && !list->count && !list->size);

  M_Check(printer_owner->used >= list->offset);
  list->size = printer_owner->used - list->offset;

  U32 type_size = TYPE_GetSize(list->type);
  M_Check(list->size % type_size == 0);
  list->count = list->size / type_size;
}
static void *BREAD_ListReserveBytes(Printer *p, BREAD_List *list, TYPE_ENUM type, U32 count)
{
  BREAD_ListStart(p, list, type);
  void *result = BREAD_ReserveBytes(p, TYPE_GetSize(type)*count, TYPE_GetAlign(type));
  BREAD_ListEnd(p, list);
  return result;
}
#define BREAD_ListReserve(P, List, Type, Count) (Type *)BREAD_ListReserveBytes(P, List, TYPE_##Type, Count)

//
// Builder create and finalize
//
static BREAD_Builder BREAD_CreateBuilder(Arena *a, U32 max_file_size)
{
  BREAD_Builder bb = {};
  bb.file = Pr_Alloc(a, max_file_size);
  U32 half_size = max_file_size / 2;
  bb.rigid_vertices = Pr_Alloc(a, half_size);
  bb.skinned_vertices = Pr_Alloc(a, half_size);
  bb.indices = Pr_Alloc(a, half_size);
  bb.skeletons = Pr_Alloc(a, half_size);
  bb.materials = Pr_Alloc(a, half_size);

  Pr_ReserveBytes(&bb.file, sizeof(BREAD_Header)); // reserve space for header
  bb.selected_model = MODEL_COUNT;
  return bb;
}

static void BREAD_FinalizeBuilder(BREAD_Builder *bb)
{
  bb->selected_model = MODEL_COUNT;

  //
  // Prepare BREAD_Links and append it to memory
  //
  BREAD_Links links = {};

  BREAD_ListStart(&bb->file, &links.models.rigid_vertices, TYPE_WORLD_GpuRigidVertex);
  Pr_Printer(&bb->file, &bb->rigid_vertices);
  BREAD_ListEnd(&bb->file, &links.models.rigid_vertices);

  BREAD_ListStart(&bb->file, &links.models.skinned_vertices, TYPE_WORLD_GpuSkinnedVertex);
  Pr_Printer(&bb->file, &bb->skinned_vertices);
  BREAD_ListEnd(&bb->file, &links.models.skinned_vertices);

  BREAD_ListStart(&bb->file, &links.models.indices, TYPE_U16);
  Pr_Printer(&bb->file, &bb->indices);
  BREAD_ListEnd(&bb->file, &links.models.indices);

  ForArray(i, bb->models)
  {
    BREAD_Model *model = bb->models + i;
    if (!model->indices_count)
    {
      M_LOG(M_LogGltfWarning, "BREAD warning: When finalizing file "
            "found model with index %d (%s) that wasn't initialized.",
            i, MODEL_GetCstrName(i));
    }
  }

  // Models
  BREAD_ListStart(&bb->file, &links.models.list, TYPE_BREAD_Model);
  {
    BREAD_Model *dst = BREAD_ReserveBytes(&bb->file, sizeof(bb->models), _Alignof(BREAD_Model));
    Memcpy(dst, bb->models, sizeof(bb->models));
  }
  BREAD_ListEnd(&bb->file, &links.models.list);

  // Skeletons
  BREAD_ListStart(&bb->file, &links.skeletons, TYPE_BREAD_Skeleton);
  Pr_Printer(&bb->file, &bb->skeletons);
  BREAD_ListEnd(&bb->file, &links.skeletons);

  // Materials
  BREAD_ListStart(&bb->file, &links.materials, TYPE_BREAD_Material);
  Pr_Printer(&bb->file, &bb->materials);
  BREAD_ListEnd(&bb->file, &links.materials);

  //
  // Prepare BREAD_Header
  //
  BREAD_Header *header = (BREAD_Header *)bb->file.buf;
  *BREAD_ListReserve(&bb->file, &header->links, BREAD_Links, 1) = links;

  //
  // Calculate hash
  //
  S8 whole_file = Pr_AsS8(&bb->file);
  S8 hashable_file = S8_Skip(whole_file, sizeof(header->file_hash));
  header->file_hash = S8_Hash(BREAD_MAGIC_HASH_SEED, hashable_file);

  bb->finalized = true;
}

static void BREAD_SaveToFile(BREAD_Builder *bb, const char *file_path)
{
  M_Check(bb->finalized);
  S8 content = Pr_AsS8(&bb->file);
  M_SaveFile(file_path, content);
}

//
// Helpers
//
static void BREAD_AddModel(BREAD_Builder *bb, MODEL_Type model_type, bool is_skinned)
{
  M_Check(!bb->finalized);
  M_Check(model_type < MODEL_COUNT);
  bb->selected_model = model_type;

  BREAD_Model *model = bb->models + bb->selected_model;
  model->is_skinned = is_skinned;
  // Check that model wasn't initialized already
  M_Check(!model->is_init_vertices &&
          !model->is_init_indices &&
          !model->indices_count);
}

static void *BREAD_AddModelVertex(BREAD_Builder *bb, bool is_skinned)
{
  M_Check(!bb->finalized);
  Printer *vert_printer = is_skinned ? &bb->skinned_vertices : &bb->rigid_vertices;
  U64 vert_align = is_skinned ? _Alignof(WORLD_GpuSkinnedVertex) : _Alignof(WORLD_GpuRigidVertex);
  U64 vert_size = is_skinned ? sizeof(WORLD_GpuSkinnedVertex) : sizeof(WORLD_GpuRigidVertex);

  M_Check(bb->selected_model < MODEL_COUNT);
  BREAD_Model *model = bb->models + bb->selected_model;

  // init vertices offset
  if (!model->is_init_vertices)
  {
    model->is_init_vertices = true;
    model->vertices_start_index = vert_printer->used / vert_size;
  }

  void *result = BREAD_ReserveBytes(vert_printer, vert_size, vert_align);
  Memclear(result, vert_size);
  return result;
}
static WORLD_GpuSkinnedVertex *BREAD_AddModelSkinnedVertex(BREAD_Builder *bb) { return BREAD_AddModelVertex(bb, true); }
static WORLD_GpuRigidVertex   *BREAD_AddModelRigidVertex(BREAD_Builder *bb)   { return BREAD_AddModelVertex(bb, false); }

static void BREAD_CopyIndices(BREAD_Builder *bb, U16 *src_indices, U32 src_indices_count)
{
  M_Check(!bb->finalized);
  M_Check(bb->selected_model < MODEL_COUNT);
  BREAD_Model *model = bb->models + bb->selected_model;

  if (!model->is_init_indices)
  {
    model->is_init_indices = true;
    model->indices_start_index = bb->indices.used / sizeof(U16);
  }
  model->indices_count += src_indices_count;

  U16 *dst = BREAD_Reserve(&bb->indices, U16, src_indices_count);
  Memcpy(dst, src_indices, sizeof(U16)*src_indices_count);
}
