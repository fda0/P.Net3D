typedef struct
{
  Printer file;
  bool finalized;

  Printer rigid_vertices;
  Printer skinned_vertices;
  Printer indices;
  BREAD_Model models[MODEL_COUNT];
  MODEL_Type selected_model;

  BREAD_Skeleton skeletons[1024];
  U32 skeletons_count;
} BREAD_Builder;

typedef struct
{
  Arena *tmp;
  Arena *cgltf_arena;
} BAKER_State;
static BAKER_State BAKER;
