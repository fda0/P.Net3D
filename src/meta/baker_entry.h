typedef struct
{
  Printer file;
  Printer rigid_vertices;
  Printer skinned_vertices;
  Printer indices;

  BREAD_Model models[MDL_COUNT];
  MDL_Kind selected_model;

  bool finalized;
} BREAD_Builder;

typedef struct
{
  Arena *tmp;
  Arena *cgltf_arena;
} BAKER_State;
static BAKER_State BAKER;
