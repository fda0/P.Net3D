typedef enum
{
  GPU_MemoryUnknown,
  GPU_MemoryMeshVertices, // MATERIAL_Key
  GPU_MemoryModelInstances, // MODEL_COUNT
  GPU_MemoryJointTransforms, // 1
} GPU_MemoryTargetType;

typedef struct
{
  GPU_MemoryTargetType type;
  MATERIAL_Key material_key;
  MODEL_Type model;
} GPU_MemoryTarget;

typedef struct GPU_Transfer GPU_Transfer;
struct GPU_Transfer
{
  SDL_GPUTransferBuffer *handle;
  U32 cap; // in bytes

  U32 used; // in bytes
  void *mapped_memory;
  GPU_Transfer *next;
};

typedef struct GPU_Buffer GPU_Buffer;
struct GPU_Buffer
{
  SDL_GPUBuffer *handle;
  U32 cap;

  GPU_Buffer *next; // used by free list only
};

typedef struct
{
  GPU_MemoryTarget target;

  // cleared after every frame
  GPU_Transfer *transfer_first;
  GPU_Transfer *transfer_last;
  U32 total_used;
  U32 element_count;

  // can live between frames
  GPU_Buffer *buffer;
} GPU_MemoryBundle;

typedef struct
{
  Arena *arena; // for allocating GPU_TransferStorage, GPU_BufferStorage
  // these free list don't hold any GPU resources, just a way to reuse CPU memory
  GPU_Transfer *free_transfers;
  GPU_Buffer *free_buffers;

  GPU_MemoryBundle bundles[2048];
  U32 bundles_count;
} GPU_MemoryState;
