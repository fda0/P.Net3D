typedef struct
{
  float scale;
  Quat rot;
  V3 move;
} BK_GLTF_ModelConfig;

typedef struct
{
  BK_GLTF_ModelConfig config;

  BK_Buffer indices;
  BK_Buffer positions;
  BK_Buffer normals;
  BK_Buffer texcoords;
  BK_Buffer joint_indices;
  BK_Buffer weights;
  BK_Buffer colors;

  S8 name; // @todo remove
  MODEL_Type type;
  bool is_skinned;
  U32 verts_count;
  U32 joints_count;

  bool has_skeleton;
  U32 skeleton_index;
} BK_GLTF_ModelData;
