static void *BREAD_ReserveBytes(BREAD_Builder *bb, U64 size, U64 alignment)
{
  M_Check(bb->p.used % alignment == 0);
  void *result = Pr_ReserveBytes(&bb->p, size);
  M_Check(result);
  return result;
}
#define BREAD_Reserve(bb, Type, Count) (Type *)BREAD_ReserveBytes(bb, sizeof(Type)*(Count), _Alignof(Type))

static BREAD_Builder BREAD_CreateBuilder(Arena *a, U32 max_file_size)
{
  BREAD_Builder bb = {};
  bb.p = Pr_Alloc(a, max_file_size);
  Pr_ReserveBytes(&bb.p, sizeof(BREAD_Header)); // reserve space for header
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

  // Models array - validate
  ForArray(i, bb->models)
  {
    BREAD_Model *model = bb->models + i;
    if (!model->vertices.count || !model->indices.count)
    {
      M_LOG(M_LogGltfWarning, "BREAD warning: "
            "When finalizing file found model with index %d that wasn't initialized.", i);
    }
  }

  // Models array - put in memory
  contents.models.offset = bb->p.used;
  contents.models.count = ArrayCount(bb->models);
  {
    BREAD_Model *dst = BREAD_ReserveBytes(bb, sizeof(bb->models), _Alignof(BREAD_Model));
    Memcpy(dst, bb->models, sizeof(bb->models));
  }

  //
  // Prepare BREAD_Header
  //
  BREAD_Header *header = (BREAD_Header *)bb->p.buf;
  header->contents_offset = bb->p.used;
  *BREAD_Reserve(bb, BREAD_Contents, 1) = contents;

  //
  // Calculate hash
  //
  S8 whole_file = Pr_AsS8(&bb->p);
  S8 hashable_file = S8_Skip(whole_file, sizeof(header->file_hash));
  header->file_hash = S8_Hash(BREAD_MAGIC_HASH_SEED, hashable_file);

  bb->finalized = true;
}

static void BREAD_SaveToFile(BREAD_Builder *bb, const char *file_path)
{
  M_Check(bb->finalized);
  S8 content = Pr_AsS8(&bb->p);
  M_SaveFile(file_path, content);
}

static void BREAD_AddModel(BREAD_Builder *bb, MDL_Kind model_kind, bool is_skinned)
{
  M_Check(!bb->finalized);
  M_Check(model_kind < MDL_COUNT);
  bb->selected_model = model_kind;

  BREAD_Model *model = bb->models + bb->selected_model;
  model->is_skinned = is_skinned;
  // make sure model wasn't initialized already
  M_Check(!model->vertices.offset && !model->skeleton_offset);
}

static void *BREAD_AddModelVertex(BREAD_Builder *bb, bool is_skinned)
{
  M_Check(!bb->finalized);
  U64 align = is_skinned ? _Alignof(MDL_GpuSkinnedVertex) : _Alignof(MDL_GpuRigidVertex);
  U64 size = is_skinned ? sizeof(MDL_GpuSkinnedVertex) : sizeof(MDL_GpuRigidVertex);

  M_Check(bb->selected_model < MDL_COUNT);
  BREAD_Model *model = bb->models + bb->selected_model;
  if (!model->vertices.offset)
  {
    // init vertices offset
    model->vertices.offset = bb->p.used;
  }
  else
  {
    // check if memory is continous
    M_Check(model->vertices.offset + model->vertices.count*size == bb->p.used);
  }

  model->vertices.count += 1;

  void *result = BREAD_ReserveBytes(bb, size, align);
  Memclear(result, size);
  return result;
}

static MDL_GpuSkinnedVertex *BREAD_AddModelSkinnedVertex(BREAD_Builder *bb)
{
  return BREAD_AddModelVertex(bb, true);
}

static MDL_GpuRigidVertex *BREAD_AddModelRigidVertex(BREAD_Builder *bb)
{
  return BREAD_AddModelVertex(bb, false);
}

static void BREAD_CopyIndices(BREAD_Builder *bb, U16 *src_indices, U64 src_indices_count)
{
  M_Check(!bb->finalized);
  M_Check(bb->selected_model < MDL_COUNT);
  BREAD_Model *model = bb->models + bb->selected_model;

  M_Check(!model->indices.offset);
  model->indices.offset = bb->p.used;
  model->indices.count = src_indices_count;

  U16 *dst = BREAD_Reserve(bb, U16, src_indices_count);
  Memcpy(dst, src_indices, sizeof(U16)*src_indices_count);
}
