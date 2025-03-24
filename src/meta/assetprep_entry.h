typedef struct
{
  RDR_RigidVertex rdr;
  bool filled;
} M_VertexEntry;

typedef struct
{
  Arena *tmp;
  Arena *cgltf_arena;

  M_VertexEntry vertex_table[1024*1024];
} Meta_State;
static Meta_State M;
