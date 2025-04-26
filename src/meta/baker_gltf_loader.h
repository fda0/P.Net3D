typedef struct
{
  float scale;
  Quat rot;
  V3 move;
} BK_GLTF_Config;

typedef struct
{
  BK_Buffer indices;
  BK_Buffer positions;
  BK_Buffer normals;
  BK_Buffer texcoords;
  BK_Buffer joint_indices;
  BK_Buffer weights;
  BK_Buffer colors;

  S8 name; // @todo remove
  MDL_Kind kind;
  bool is_skinned;
  U32 verts_count;
  U32 joints_count;
} BK_GLTF_ModelData;
