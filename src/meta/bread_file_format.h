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
  BREAD_Range root_transform; // [Mat4]

  U32 joints_count;
  BREAD_Range inv_bind_mats;      // [Mat4] * joints_count
  BREAD_Range child_index_buf;    // [U32] * joints_count
  BREAD_Range child_index_ranges; // [RngU32] * joints_count
  BREAD_Range translations;       // [V3] * joints_count
  BREAD_Range rotations;          // [Quat] * joints_count
  BREAD_Range scales;             // [V3] * joints_count
  BREAD_Range name_ranges;        // [RngU32] * joints_count - offset of min & max char* - can be transformed to S8

  // struct
  // {
  //   Mat4 inverse_bind_matrices[joints_count];
  //   U32 child_index_buf[joints_count];
  //   RngU32 child_index_ranges[joints_count];
  //   V3 translations[joints_count];
  //   Quat rotations[joints_count];
  //   V3 scales[joints_count];
  //   BREAD_Range names[joints_count]; // can be converted directly into S8 type
  // } joints_data;

  BREAD_Range animations; // [@todo]
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
// [BREAD_Header]
// all rigid vertices [MDL_GpuRigidVertex]
// all skinned vertices [MDL_GpuSkinnedVertex]
// all indices [U16]
// array of [BREAD_Model]
// [BREAD_Contents]
//
