typedef struct
{
  float scale;
  Mat4 rot;
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

  S8 name;
  bool is_skinned;
  U32 vert_count;
  U32 joints_count;
} BK_GLTF_ModelData;
