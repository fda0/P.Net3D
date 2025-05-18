typedef enum
{
  GPU_MEM_Unknown,
  GPU_MEM_MeshVertices, // MATERIAL_Key
  GPU_MEM_ModelInstances, // MODEL_COUNT
} GPU_MEM_TargetType;

typedef struct
{
  GPU_MEM_TargetType type;
  MATERIAL_Key material_key;
  MODEL_Key model_key;
} GPU_MEM_Target;

typedef struct GPU_MEM_Transfer GPU_MEM_Transfer;
struct GPU_MEM_Transfer
{
  SDL_GPUTransferBuffer *handle;
  U32 cap; // in bytes

  U32 used; // in bytes
  void *mapped_memory;
  GPU_MEM_Transfer *next;
};

typedef struct GPU_MEM_Buffer GPU_MEM_Buffer;
struct GPU_MEM_Buffer
{
  SDL_GPUBuffer *handle;
  U32 cap;

  GPU_MEM_Buffer *next; // used by free list only
};

typedef struct
{
  GPU_MEM_Target target;

  // cleared after every frame
  GPU_MEM_Transfer *transfer_first;
  GPU_MEM_Transfer *transfer_last;
  U32 total_used;
  U32 element_count;

  // can live between frames
  GPU_MEM_Buffer *buffer;
} GPU_MEM_Batch;

typedef struct
{
  Arena *arena; // for allocating GPU_TransferStorage, GPU_BufferStorage
  // these free list don't hold any GPU resources, just a way to reuse CPU memory
  GPU_MEM_Transfer *free_transfers;
  GPU_MEM_Buffer *free_buffers;

  GPU_MEM_Batch poses;
  GPU_MEM_Batch batches[2048];
  U32 batches_count;
} GPU_MEM_State;
