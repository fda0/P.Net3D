//
// This file describes BREAD file format - which is produced by "baker" asset preprocessor.
//
// _offset -> offsets are like pointers but are relative to the first byte of the file
//

typedef struct
{
  U32 offset;
  U32 size;
  U32 elem_count;
} BREAD_Range;

typedef enum
{
  AN_Translation,
  AN_Rotation,
  AN_Scale,
} AN_TransformType;

typedef struct
{
  U32 joint_index : 30;
  AN_TransformType type : 2;
  BREAD_Range inputs;  // [V3 or Quat] * count
  BREAD_Range outputs; // [V3 or Quat] * count
} BREAD_AnimChannel;

typedef struct
{
  BREAD_Range name_string; // [U8]
  BREAD_Range channels; // [BREAD_AnimChannel]
  float t_min, t_max;
} BREAD_Animation;

typedef struct
{
  BREAD_Range root_transform; // [Mat4]

  BREAD_Range inv_bind_mats;      // [Mat4] * joints_count
  BREAD_Range child_index_buf;    // [U32] * joints_count
  BREAD_Range child_index_ranges; // [RngU32] * joints_count
  BREAD_Range translations;       // [V3] * joints_count
  BREAD_Range rotations;          // [Quat] * joints_count
  BREAD_Range scales;             // [V3] * joints_count
  BREAD_Range name_ranges;        // [RngU32] * joints_count - offset of min & max char* - can be transformed to S8

  BREAD_Range animations; // [BREAD_Animation] array
} BREAD_Skeleton;

typedef struct
{
  U8 is_skinned : 1;
  U8 is_init_vertices : 1;
  U8 is_init_indices : 1;
  U32 vertices_start_index;
  U32 indices_start_index;
  U32 indices_count;
  U32 skeleton_index; // [BREAD_Skeleton]; for skinned only; index to skeletons from BREAD_Contents
} BREAD_Model;

typedef struct
{
  struct
  {
    BREAD_Range rigid_vertices; // [WORLD_GpuRigidVertex] array
    BREAD_Range skinned_vertices; // [WORLD_GpuSkinnedVertex] array
    BREAD_Range indices; // [U16] array
    BREAD_Range list; // [BREAD_Model] array
  } models;

  BREAD_Range skeletons; // [BREAD_Skeleton] array
} BREAD_Links;

typedef struct
{
  U64 file_hash; // of everything after itself (the first 8 bytes) - seeded with BREAD_MAGIC_HASH_SEED
#define BREAD_MAGIC_HASH_SEED (0xB5'EA'DC'0D + 0)
  BREAD_Range links; // [BREAD_Links]
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
