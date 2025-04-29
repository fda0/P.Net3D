#define GPU_MEMORY_TARGET_COUNT (TEX_COUNT + MDL_COUNT + 1/*joints_count*/)

typedef enum
{
  GPU_MemoryUnknown,
  GPU_MemoryMeshVertices, // TEX_COUNT
  GPU_MemoryModelInstances, // MDL_COUNT
  GPU_MemoryJointTransforms, // 1
} GPU_MemoryTargetType;

typedef struct
{
  GPU_MemoryTargetType type;
  TEX_Kind tex;
  MDL_Kind model;
} GPU_MemoryTarget;

typedef struct GPU_TransferStorage GPU_TransferStorage;
struct GPU_TransferStorage
{
  SDL_GPUTransferBuffer *handle;
  U32 cap; // in bytes

  U32 used; // in bytes
  void *mapped_memory;
  GPU_TransferStorage *next;
};

typedef struct GPU_BufferStorage GPU_BufferStorage;
struct GPU_BufferStorage
{
  SDL_GPUBuffer *handle;
  U32 cap;

  GPU_BufferStorage *next; // used by free list only
};

typedef struct
{
  // cleared after every frame
  GPU_TransferStorage *transfer_first;
  GPU_TransferStorage *transfer_last;
  U32 total_used;
  U32 element_count;

  // can live between frames
  GPU_BufferStorage *buffer;
} GPU_MemoryEntry;

typedef struct
{
  Arena *arena; // for allocating GPU_TransferStorage, GPU_BufferStorage
  // these free list don't hold any GPU resources, just a way to reuse CPU memory
  GPU_TransferStorage *free_transfers;
  GPU_BufferStorage *free_buffers;

  GPU_MemoryEntry entries[GPU_MEMORY_TARGET_COUNT];
} GPU_MemoryState;
