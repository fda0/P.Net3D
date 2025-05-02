//
// Allocating bytes in printer
//
static void *BREAD_ReserveBytes(Printer *p, U64 size, U64 alignment)
{
  M_Check(p->used % alignment == 0);
  void *result = Pr_ReserveBytes(p, size);
  M_Check(result);
  M_Check(!p->err);
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
static void BREAD_RangeStart(Printer *printer_owner, BREAD_Range *range)
{
  range->offset = printer_owner->used;
}
static void BREAD_RangeEnd(Printer *printer_owner, BREAD_Range *range, U32 elem_count)
{
  M_Check(printer_owner->used >= range->offset);
  range->size = printer_owner->used - range->offset;
  range->elem_count = elem_count;
}
static void *BREAD_ReserveBytesToRange(Printer *p, BREAD_Range *range, U64 size, U64 alignment, U32 elem_count)
{
  BREAD_RangeStart(p, range);
  void *result = BREAD_ReserveBytes(p, size, alignment);
  BREAD_RangeEnd(p, range, elem_count);
  return result;
}
#define BREAD_ReserveToRange(P, Range, Type, Count) (Type *)BREAD_ReserveBytesToRange(P, Range, sizeof(Type)*(Count), _Alignof(Type), (Count))

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

  Pr_ReserveBytes(&bb.file, sizeof(BREAD_Header)); // reserve space for header
  bb.selected_model = MODEL_COUNT;
  return bb;
}

static void BREAD_FinalizeBuilder(BREAD_Builder *bb)
{
  bb->selected_model = MODEL_COUNT;

  //
  // Prepare BREAD_Contents and append it to memory
  //
  BREAD_Contents contents = {};

  contents.models.rigid_vertices.offset = bb->file.used;
  contents.models.rigid_vertices.size = bb->rigid_vertices.used;
  contents.models.rigid_vertices.elem_count = bb->rigid_vertices.used / sizeof(WORLD_GpuRigidVertex);
  Pr_Printer(&bb->file, &bb->rigid_vertices);

  contents.models.skinned_vertices.offset = bb->file.used;
  contents.models.skinned_vertices.size = bb->skinned_vertices.used;
  contents.models.skinned_vertices.elem_count = bb->skinned_vertices.used / sizeof(WORLD_GpuSkinnedVertex);
  Pr_Printer(&bb->file, &bb->skinned_vertices);

  contents.models.indices.offset = bb->file.used;
  contents.models.indices.size = bb->indices.used;
  contents.models.indices.elem_count = bb->indices.used / sizeof(U16);
  Pr_Printer(&bb->file, &bb->indices);

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

  // Models array - put in memory
  contents.models.list.offset = bb->file.used;
  contents.models.list.size = sizeof(bb->models);
  contents.models.list.elem_count = ArrayCount(bb->models);
  {
    BREAD_Model *dst = BREAD_ReserveBytes(&bb->file, sizeof(bb->models), _Alignof(BREAD_Model));
    Memcpy(dst, bb->models, sizeof(bb->models));
  }

  //
  // Prepare BREAD_Header
  //
  BREAD_Header *header = (BREAD_Header *)bb->file.buf;
  header->contents_offset = bb->file.used;
  *BREAD_Reserve(&bb->file, BREAD_Contents, 1) = contents;

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
