//
// This file describes BREAD file format - which is produced by "baker" asset preprocessor.
//
// _offset -> offsets are like pointers but are relative to the first byte of the file
//

typedef struct
{
  U32 offset;
  U32 count;
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
  BREAD_Range vertices; // [MDL_GpuSkinnedVertex or MDL_GpuRigidVertex]
  BREAD_Range indices; // [U16]
  U32 skeleton_offset; // [BREAD_Skeleton] for skinned only
} BREAD_Model;

typedef struct
{
  BREAD_Range models; // [BREAD_Model]
} BREAD_Contents;

typedef struct
{
  U32 file_hash; // of everything after itself (the first 4 bytes) - seeded with BREAD_MAGIC_HASH_SEED
#define BREAD_MAGIC_HASH_SEED 0xB5'EA'DC'0D
  U32 contents_offset; // [BREAD_Contents]
} BREAD_Header;

//
// The format is a bit flexible in regards to where stuff is located.
// With the current BreadBuilder implementation we can expect it to be:
//
// BREAD_Header [4*3 bytes at the start]
// [buffers of vertices (MDL_GpuSkinnedVertex, MDL_GpuRigidVertex), indices (U16)]
// [continuous array of BREAD_Model]
// BREAD_Contents [4*2 bytes at the end]
//
