typedef struct
{
  SDL_GPUGraphicsPipeline *rigid;
  SDL_GPUGraphicsPipeline *skinned;
  SDL_GPUGraphicsPipeline *wall;
} GPU_WorldPipelines;

typedef struct GPU_DynamicBuffer GPU_DynamicBuffer;
struct GPU_DynamicBuffer
{
  // init once fields
  union
  {
    SDL_GPUTransferBuffer *transfer;
    SDL_GPUBuffer *final;
  };
  U32 cap_bytes;

  // can be modified fields
  U32 used_bytes;
  U32 element_count;
  void *mapped_memory; // transfer buffer only
  GPU_DynamicBuffer *next;
};

typedef enum
{
  GPU_MemoryUnknown,
  GPU_MemoryMeshVertices,
  GPU_MemoryModelInstances,
  GPU_MemoryJointTransforms,
} GPU_MemoryTarget;

typedef struct
{
  GPU_MemoryTarget target;
  TEX_Kind tex;
  MDL_Kind model;
  U32 count;
  bool final_buffer; // if false request transfer buffer
} GPU_MemorySpec;

typedef struct
{
  GPU_DynamicBuffer *buffer;
  void *offsetted_memory;
} GPU_MemoryResult;

typedef struct
{
  // IndexToSize formula: 4^(n+2) bytes.
  // 0 -> 16 bytes
  // 1 -> 64 bytes
  // 2 -> 256 bytes
  // 3 -> Kilobyte(1)
  // 4 -> Kilobyte(4)
  // 5 -> Kilobyte(16)
  // 6 -> Kilobyte(64)
  // 7 -> Kilobyte(256)
  // 8 -> Megabyte(1)
  // 9 -> Megabyte(4)
  // 10 -> Megabyte(16)
  // 11 -> Megabyte(64)
#define GPU_MEM_FREE_LIST_SIZE 12
  GPU_DynamicBuffer *free_list[GPU_MEM_FREE_LIST_SIZE];

  // used
  GPU_DynamicBuffer *mesh_vertices[TEX_COUNT];
  GPU_DynamicBuffer *model_instances[MDL_COUNT];
  GPU_DynamicBuffer *joint_transforms; // this probably should be broken down into batches in the future (based on update frequency?)
} GPU_MemoryBuckets;

typedef struct
{
  Arena *buffer_arena;
  GPU_MemoryBuckets transfer_buckets;
  GPU_MemoryBuckets final_buckets;

  // stats - not needed?
  //U32 mesh_this_frame_required_bytes[TEX_COUNT];
  //U32 model_this_frame_required_bytes[MDL_COUNT]
  //U32 mesh_max_required_bytes[TEX_COUNT];
  //U32 model_max_required_bytes[MDL_COUNT];
} GPU_MemoryManager;

typedef struct
{
  SDL_GPUDevice *device;
  GPU_MemoryManager mem;

  SDL_GPUTexture *tex_depth;
  SDL_GPUTexture *tex_msaa;
  SDL_GPUTexture *tex_resolve;

  SDL_GPUTexture *shadow_tex;
  SDL_GPUSampler *shadow_sampler;

  // pipeline, sample settings
  SDL_GPUSampleCount sample_count;
  GPU_WorldPipelines world_pipelines[2]; // 0: 4xMSAA; 1: no AA (for shadow mapping)
  SDL_GPUGraphicsPipeline *ui_pipeline;

  U64 bound_uniform_hash;
  World_GpuUniform world_uniform;



  // model, mesh, ui resources
  struct
  {
    MDL_Batch batches[MDL_COUNT];
    Mat4 poses[128 * MDL_MAX_INSTANCES * MDL_COUNT];
    U32 poses_count;

    SDL_GPUBuffer *gpu_pose_buffer;
  } model;

  struct
  {
    MSH_Batch batches[TEX_COUNT];
    SDL_GPUSampler *gpu_sampler;
  } mesh;

  struct
  {
    struct
    {
      SDL_GPUTexture *atlas_texture;
      SDL_GPUSampler *atlas_sampler;
      SDL_GPUBuffer *indices;
      SDL_GPUBuffer *shape_buffer;
      SDL_GPUBuffer *clip_buffer;
    } gpu;

    I32 indices[1024 * 8];
    U32 indices_count;

    UI_GpuShape shapes[1024 * 2];
    U32 shapes_count;

    UI_GpuClip clips[1024];
    U32 clips_count;
    U32 clip_stack[256];
    U32 clip_stack_index;
  } ui;

  // sdl properties
  SDL_PropertiesID clear_depth_props;
  SDL_PropertiesID clear_color_props;
} GPU_State;
