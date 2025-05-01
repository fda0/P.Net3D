static void *BREAD_ReserveBytes(Printer *p, U64 size, U64 alignment)
{
  M_Check(p->used % alignment == 0);
  void *result = Pr_ReserveBytes(p, size);
  M_Check(result);
  M_Check(!p->err);
  return result;
}
#define BREAD_Reserve(P, Type, Count) (Type *)BREAD_ReserveBytes(P, sizeof(Type)*(Count), _Alignof(Type))

static BREAD_Builder BREAD_CreateBuilder(Arena *a, U32 max_file_size)
{
  BREAD_Builder bb = {};
  bb.file = Pr_Alloc(a, max_file_size);
  bb.rigid_vertices = Pr_Alloc(a, max_file_size / 2);
  bb.skinned_vertices = Pr_Alloc(a, max_file_size / 2);
  bb.indices = Pr_Alloc(a, max_file_size / 2);

  Pr_ReserveBytes(&bb.file, sizeof(BREAD_Header)); // reserve space for header
  bb.selected_model = MDL_COUNT;
  return bb;
}

static void BREAD_FinalizeBuilder(BREAD_Builder *bb)
{
  bb->selected_model = MDL_COUNT;

  //
  // Prepare BREAD_Contents and append it to memory
  //
  BREAD_Contents contents = {};

  contents.models.rigid_vertices.offset = bb->file.used;
  contents.models.rigid_vertices.size = bb->rigid_vertices.used;
  contents.models.rigid_vertices.elem_count = bb->rigid_vertices.used / sizeof(MDL_GpuRigidVertex);
  Pr_Printer(&bb->file, &bb->rigid_vertices);

  contents.models.skinned_vertices.offset = bb->file.used;
  contents.models.skinned_vertices.size = bb->skinned_vertices.used;
  contents.models.skinned_vertices.elem_count = bb->skinned_vertices.used / sizeof(MDL_GpuSkinnedVertex);
  Pr_Printer(&bb->file, &bb->skinned_vertices);

  contents.models.indices.offset = bb->file.used;
  contents.models.indices.size = bb->indices.used;
  contents.models.indices.elem_count = bb->indices.used / sizeof(U16);
  Pr_Printer(&bb->file, &bb->indices);

  ForArray(i, bb->models)
  {
    BREAD_Model *model = bb->models + i;
    if (!model->vertices.size || !model->indices.size)
    {
      M_LOG(M_LogGltfWarning, "BREAD warning: When finalizing file "
            "found model with index %d (%s) that wasn't initialized.",
            i, MDL_GetCstrName(i));
    }

    model->vertices.offset += (model->is_skinned ?
                               contents.models.skinned_vertices.offset :
                               contents.models.rigid_vertices.offset);
    model->indices.offset += contents.models.indices.offset;
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
static void BREAD_AddModel(BREAD_Builder *bb, MDL_Kind model_kind, bool is_skinned)
{
  M_Check(!bb->finalized);
  M_Check(model_kind < MDL_COUNT);
  bb->selected_model = model_kind;

  BREAD_Model *model = bb->models + bb->selected_model;
  model->is_skinned = is_skinned;
  // make sure model wasn't initialized already
  M_Check(!model->vertices.offset);
}

static void *BREAD_AddModelVertex(BREAD_Builder *bb, bool is_skinned)
{
  M_Check(!bb->finalized);
  Printer *vert_printer = is_skinned ? &bb->skinned_vertices : &bb->rigid_vertices;
  U64 vert_align = is_skinned ? _Alignof(MDL_GpuSkinnedVertex) : _Alignof(MDL_GpuRigidVertex);
  U64 vert_size = is_skinned ? sizeof(MDL_GpuSkinnedVertex) : sizeof(MDL_GpuRigidVertex);

  M_Check(bb->selected_model < MDL_COUNT);
  BREAD_Model *model = bb->models + bb->selected_model;

  // init vertices offset
  if (!model->vertices.size)
    model->vertices.offset = vert_printer->used;

  // check if memory is continous
  M_Check(model->vertices.offset + model->vertices.size == vert_printer->used);

  model->vertices.size += vert_size;
  model->vertices.elem_count += 1;
  model->vertices_start_index = model->vertices.offset / vert_size;
  M_Check(model->vertices.size == vert_size * model->vertices.elem_count);

  void *result = BREAD_ReserveBytes(vert_printer, vert_size, vert_align);
  Memclear(result, vert_size);
  return result;
}
static MDL_GpuSkinnedVertex *BREAD_AddModelSkinnedVertex(BREAD_Builder *bb) { return BREAD_AddModelVertex(bb, true); }
static MDL_GpuRigidVertex   *BREAD_AddModelRigidVertex(BREAD_Builder *bb)   { return BREAD_AddModelVertex(bb, false); }

static void BREAD_CopyIndices(BREAD_Builder *bb, U16 *src_indices, U32 src_indices_count)
{
  M_Check(!bb->finalized);
  M_Check(bb->selected_model < MDL_COUNT);
  BREAD_Model *model = bb->models + bb->selected_model;

  M_Check(!model->indices.offset);
  model->indices.offset = bb->indices.used;
  model->indices.size = src_indices_count*sizeof(U16);
  model->indices.elem_count = src_indices_count;
  model->indices_start_index = model->indices.offset / sizeof(U16);

  U16 *dst = BREAD_Reserve(&bb->indices, U16, src_indices_count);
  Memcpy(dst, src_indices, sizeof(U16)*src_indices_count);
}
