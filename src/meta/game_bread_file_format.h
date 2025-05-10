//
// This file describes BREAD file format - which is produced by "baker" asset preprocessor.
//
// _offset -> offsets are like pointers but are relative to the first byte of the file
//
// @todo increase typesafety of this program
//

typedef struct
{
  U32 type;
  U32 count;

  U32 offset;
  U32 size;
} BREAD_List;

#define BREAD_ListT(TYPE) BREAD_List

typedef enum
{
  AN_Translation,
  AN_Rotation,
  AN_Scale,
} BREAD_TransformType;

typedef struct
{
  U32 joint_index : 30;
  U32 type : 2; // [BREAD_TransformType]
  BREAD_ListT(float) inputs;  // float * count
  BREAD_ListT(float) outputs; // float * (3 or 4 [V3 or Quat]) * count
} BREAD_AnimationChannel;

typedef struct
{
  BREAD_ListT(U8) name;
  float t_min, t_max;
  BREAD_ListT(BREAD_AnimationChannel) channels;
} BREAD_Animation;

typedef struct
{
  // each of these has the same count of elements (joints_count)
  BREAD_ListT(Mat4)   inv_bind_mats;
  BREAD_ListT(U32)    child_index_buf;
  BREAD_ListT(RngU32) child_index_ranges;
  BREAD_ListT(V3)     translations;
  BREAD_ListT(Quat)   rotations;
  BREAD_ListT(V3)     scales;
  BREAD_ListT(RngU32) name_ranges; // offset of min & max char* - can be transformed to S8

  BREAD_ListT(BREAD_Animation) anims;
  Mat4 root_transform;
} BREAD_Skeleton;

typedef struct
{
  U8 is_skinned : 1;
  U8 is_init_vertices : 1;
  U8 is_init_indices : 1;

  U32 vertices_start_index;
  U32 indices_start_index;
  U32 indices_count;
  U32 skeleton_index; // for skinned only; index to skeletons from BREAD_Contents

  // @todo add [U8] name field
} BREAD_Model;

typedef struct
{
  U32 width;
  U32 height;
  U32 lod;
  U32 layer;
  // relative to full_data of Material
  U32 data_offset;
  U32 data_size;
} BREAD_MaterialSection;

typedef struct
{
  U32 width;
  U32 height;
  U32 lods;
  U32 layers;
  BREAD_ListT(U8) full_data;
  BREAD_ListT(BREAD_MaterialSection) sections;
} BREAD_Material;

typedef struct
{
  struct
  {
    BREAD_ListT(WORLD_GpuRigidVertex) rigid_vertices;
    BREAD_ListT(WORLD_GpuSkinnedVertex) skinned_vertices;
    BREAD_ListT(U16) indices;
    BREAD_ListT(BREAD_Model) list;
  } models;

  BREAD_ListT(BREAD_Skeleton) skeletons;
  BREAD_ListT(BREAD_Material) materials;
} BREAD_Links;

typedef struct
{
  U64 file_hash; // of everything after itself (the first 8 bytes) - seeded with BREAD_MAGIC_HASH_SEED
#define BREAD_MAGIC_HASH_SEED (0xB5EADC0D + 0)
  BREAD_List links;
} BREAD_Header;

//
// The format is a bit flexible in regards to where stuff is located.
// With the current BreadBuilder implementation we can expect it to be:
//
// [BREAD_Header]
// all rigid vertices [MDL_GpuRigidVertex]
// all skinned vertices [MDL_GpuSkinnedVertex]
// all indices [U16]
// array of [BREAD_Model]
// [BREAD_Links]
//
