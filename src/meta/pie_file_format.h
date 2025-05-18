//
// This file describes PIE file format - which is produced by "baker" asset preprocessor.
// .pie files pack all the data needed by the game like models' meshes & animations, materials, textures.
//

typedef struct
{
  U32 type;
  U32 count;

  U32 offset;
  U32 size;
} PIE_List;

#define PIE_ListT(TYPE) PIE_List

typedef enum
{
  AN_Translation,
  AN_Rotation,
  AN_Scale,
} PIE_TransformType;

typedef struct
{
  U32 joint_index : 30;
  U32 type : 2; // [PIE_TransformType]
  PIE_ListT(float) inputs;  // float * count
  PIE_ListT(float) outputs; // float * (3 or 4 [V3 or Quat]) * count
} PIE_AnimationChannel;

typedef struct
{
  PIE_ListT(U8) name;
  float t_min, t_max;
  PIE_ListT(PIE_AnimationChannel) channels;
} PIE_Animation;

typedef struct
{
  // each of these has the same count of elements (joints_count)
  PIE_ListT(Mat4)   inv_bind_mats;
  PIE_ListT(U32)    child_index_buf;
  PIE_ListT(RngU32) child_index_ranges;
  PIE_ListT(V3)     translations;
  PIE_ListT(Quat)   rotations;
  PIE_ListT(V3)     scales;
  PIE_ListT(RngU32) name_ranges; // offset of min & max char* - can be transformed to S8

  PIE_ListT(PIE_Animation) anims;
  Mat4 root_transform;
} PIE_Skeleton;

typedef struct
{
  U32 material_index;

  U32 vertices_start_index;
  U32 indices_start_index;
  U32 indices_count;
} PIE_Mesh;

typedef struct
{
  PIE_ListT(U8) name;
  U8 is_skinned : 1;
  U32 skeleton_index; // for skinned only
  PIE_ListT(PIE_Mesh) meshes;
} PIE_Model;

typedef struct
{
  U32 width;
  U32 height;
  U32 lod;
  U32 layer;
  // relative to full_data of Material
  U32 data_offset;
  U32 data_size;
} PIE_MaterialTexSection;

typedef U32 PIE_TexFormat;
enum
{
  PIE_Tex_Empty,
  PIE_Tex_R8G8B8A8,
  PIE_Tex_BC7_RGBA,
};

typedef U32 PIE_MaterialFlags;
enum
{
  PIE_MaterialFlag_HasAlpha = (1 << 0),
};

typedef struct
{
  PIE_MaterialFlags flags;
  U32 diffuse;
  U32 specular;
  float roughness; // [0:1]
} PIE_MaterialParams;

typedef struct
{
  PIE_ListT(U8) name;
  PIE_MaterialParams params;

  struct {
    PIE_TexFormat format;
    U32 width;
    U32 height;
    U32 lods;
    U32 layers;
    PIE_ListT(U8) full_data;
    PIE_ListT(PIE_MaterialTexSection) sections;
  } tex;
} PIE_Material;

typedef struct
{
  struct
  {
    PIE_ListT(WORLD_Vertex) vertices;
    PIE_ListT(U16) indices;
    PIE_ListT(PIE_Model) list;
  } models;

  PIE_ListT(PIE_Skeleton) skeletons;
  PIE_ListT(PIE_Material) materials;
} PIE_Links;

typedef struct
{
  U64 file_hash; // of everything after itself (the first 8 bytes) - seeded with PIE_MAGIC_HASH_SEED
#define PIE_MAGIC_HASH_SEED (0xB5EADC0D + 0)
  PIE_List links;
} PIE_Header;
