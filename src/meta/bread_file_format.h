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

typedef struct
{
  BREAD_Range animations; // [@todo]
  U32 joints_count;

  // BREAD_Skeleton is defined to contain after itself (it's address + sizeof(BREAD_Skeleton))
  // A bunch of arrays - each containing `joints_count` number of elements.
  //
  // V3 translations[joints_count];
  // Quat rotations[joints_count];
  // V3 scales[joints_count];
  // Mat4 inverse_bind_matrices[joints_count];
  // BREAD_Range names[joints_count]; // can be converted directly into S8 type
} BREAD_Skeleton;

typedef struct
{
  bool is_skinned;
  BREAD_Range vertices; // [MDL_GpuSkinnedVertex or MDL_GpuRigidVertex] @todo Remove this member? Old code, redundant info.
  BREAD_Range indices; // [U16] @todo Remove this member? Only indices.elem_count is needed
  U32 vertices_start_index;
  U32 indices_start_index;
  U32 skeleton_index; // [BREAD_Skeleton]; for skinned only; index to skeletons from BREAD_Contens
} BREAD_Model;

typedef struct
{
  struct
  {
    BREAD_Range rigid_vertices;
    BREAD_Range skinned_vertices;
    BREAD_Range indices;
    BREAD_Range list; // [BREAD_Model]
  } models;

  BREAD_Range skeletons_list; // [BREAD_Skeleton]
} BREAD_Contents;

typedef struct
{
  U64 file_hash; // of everything after itself (the first 8 bytes) - seeded with BREAD_MAGIC_HASH_SEED
#define BREAD_MAGIC_HASH_SEED (0xB5'EA'DC'0D + 0)
  U32 contents_offset; // [BREAD_Contents]
} BREAD_Header;

//
// The format is a bit flexible in regards to where stuff is located.
// With the current BreadBuilder implementation we can expect it to be:
//
// BREAD_Header [4*3 bytes at the start]
// all rigid vertices [MDL_GpuRigidVertex]
// all skinned vertices [MDL_GpuSkinnedVertex]
// all indices (U16)
// [continuous array of BREAD_Model]
// BREAD_Contents [4*2 bytes at the end]
//
