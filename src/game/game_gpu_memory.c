/* @todo
- Do not free and allocate transfers and buffers all the time. Reuse them.
- Think about overlapping GPU work. Now everything happens at the end of the frame in the final command buffer.
*/

static GPU_MemoryTarget GPU_MemoryIndexToTarget(U32 index)
{
  GPU_MemoryTarget target = {};
  if (index >= GPU_MEMORY_TARGET_COUNT)
    return target;

  if (index < TEX_COUNT)
  {
    target.type = GPU_MemoryMeshVertices;
    target.tex = index;
    return target;
  }
  index -= TEX_COUNT;

  if (index < MODEL_COUNT)
  {
    target.type = GPU_MemoryModelInstances;
    target.model = index;
    return target;
  }
  index -= MODEL_COUNT;

  Assert(index == 0);
  target.type = GPU_MemoryJointTransforms;
  return target;
}

static U32 GPU_MemoryTargetToBundleIndex(GPU_MemoryTarget target)
{
  U32 index = 0;
  if (target.type == GPU_MemoryMeshVertices)
    return index + target.tex;
  index += TEX_COUNT;

  if (target.type == GPU_MemoryModelInstances)
    return index + target.model;
  index += MODEL_COUNT;

  Assert(target.type == GPU_MemoryJointTransforms);
  return index;
}

static GPU_MemoryBundle *GPU_MemoryTargetToBundle(GPU_MemoryTarget target)
{
  U32 bundle_index = GPU_MemoryTargetToBundleIndex(target);
  GPU_MemoryBundle *bundle = APP.gpu.mem.bundles + bundle_index;
  return bundle;
}

static void GPU_MemoryTransferUnmap(GPU_Transfer *transfer)
{
  Assert(transfer->mapped_memory);
  SDL_UnmapGPUTransferBuffer(APP.gpu.device, transfer->handle);
  transfer->mapped_memory = 0;
}

static GPU_Transfer *GPU_MemoryTransferCreate(GPU_MemoryTarget target, U32 size)
{
  GPU_MemoryBundle *bundle = GPU_MemoryTargetToBundle(target);

  // Calculate rounded-up alloc_size
  U32 alloc_size = U32_CeilPow2(size*2);
  if (bundle->buffer)
    alloc_size = Max(alloc_size, bundle->buffer->cap);

  // Reclaim GPU_TransferStorage from free list or allocate a new one
  // Zero initialize it
  GPU_Transfer *result = APP.gpu.mem.free_transfers;
  {
    if (result)
      APP.gpu.mem.free_transfers = APP.gpu.mem.free_transfers->next;
    else
      result = Alloc(APP.gpu.mem.arena, GPU_Transfer, 1);

    Memclear(result, sizeof(*result));
  }

  // GPU alloc new transfer buffer and map it
  SDL_GPUTransferBufferCreateInfo transfer_info =
  {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = alloc_size
  };
  result->handle = SDL_CreateGPUTransferBuffer(APP.gpu.device, &transfer_info);
  result->cap = alloc_size;
  result->mapped_memory = SDL_MapGPUTransferBuffer(APP.gpu.device, result->handle, false);

  // Chain result into its GPU_MemoryEntry
  {
    if (bundle->transfer_last)
      bundle->transfer_last->next = result;

    bundle->transfer_last = result;

    if (!bundle->transfer_first)
      bundle->transfer_first = result;
  }

  return result;
}

static void *GPU_TransferGetMappedMemory(GPU_MemoryTarget target, U32 size, U32 elem_count)
{
  GPU_MemoryBundle *bundle = GPU_MemoryTargetToBundle(target);
  bundle->element_count += elem_count;

  GPU_Transfer *transfer = bundle->transfer_last;

  if (transfer)
  {
    Assert(transfer->cap > transfer->used);
    if (transfer->cap - transfer->used < size)
    {
      GPU_MemoryTransferUnmap(transfer);
      transfer = 0;
    }
  }

  if (!transfer)
    transfer = GPU_MemoryTransferCreate(target, size);

  void *result = (U8 *)transfer->mapped_memory + transfer->used;
  bundle->total_used += size;
  transfer->used += size;
  return result;
}

static void GPU_TransferUploadBytes(GPU_MemoryTarget target, void *data, U32 size, U32 elem_count)
{
  void *dest = GPU_TransferGetMappedMemory(target, size, elem_count);
  Memcpy(dest, data, size);
}

//
static void GPU_TransfersToBuffers(SDL_GPUCommandBuffer *cmd)
{
  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
  ForU32(bundle_index, GPU_MEMORY_TARGET_COUNT)
  {
    GPU_MemoryBundle *bundle = APP.gpu.mem.bundles + bundle_index;
    if (bundle->total_used)
    {
      // Check if current buffer is big enough.
      // Free it if it's too small.
      if (bundle->buffer)
      {
        if (bundle->buffer->cap < bundle->total_used)
        {
          SDL_ReleaseGPUBuffer(APP.gpu.device, bundle->buffer->handle);

          // Move BufferStorage to free list
          bundle->buffer->next = APP.gpu.mem.free_buffers;
          APP.gpu.mem.free_buffers = bundle->buffer;
          bundle->buffer = 0;
        }
      }

      // Allocate buffer
      if (!bundle->buffer)
      {
        bundle->buffer = APP.gpu.mem.free_buffers;
        {
          if (bundle->buffer)
            APP.gpu.mem.free_buffers = APP.gpu.mem.free_buffers->next;
          else
            bundle->buffer = Alloc(APP.gpu.mem.arena, GPU_Buffer, 1);

          Memclear(bundle->buffer, sizeof(*bundle->buffer));
        }

        U32 alloc_size = U32_CeilPow2(bundle->total_used*2);

        GPU_MemoryTargetType target_type = GPU_MemoryIndexToTarget(bundle_index).type;
        SDL_GPUBufferUsageFlags usage = 0;
        if (target_type == GPU_MemoryMeshVertices)
          usage |= SDL_GPU_BUFFERUSAGE_VERTEX;
        else
          usage |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;

        bundle->buffer->cap = alloc_size;
        SDL_GPUBufferCreateInfo buffer_info = {.usage = usage, .size = alloc_size};
        bundle->buffer->handle = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_info);
      }

      // Transfers -> Buffer
      U32 offset = 0;
      for (GPU_Transfer *transfer = bundle->transfer_first;
           transfer;
           transfer = transfer->next)
      {
        SDL_GPUTransferBufferLocation source = { .transfer_buffer = transfer->handle };
        SDL_GPUBufferRegion destination =
        {
          .buffer = bundle->buffer->handle,
          .offset = offset,
          .size = transfer->used
        };
        SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);

        offset += transfer->used;
      }

      // Free transfers
      GPU_Transfer *transfer = bundle->transfer_first;
      while (transfer)
      {
        GPU_Transfer *next = transfer->next;

        SDL_ReleaseGPUTransferBuffer(APP.gpu.device, transfer->handle);
        transfer->next = APP.gpu.mem.free_transfers;
        APP.gpu.mem.free_transfers = transfer;

        transfer = next;
      }

      // clear bundle transfer links
      bundle->transfer_first = 0;
      bundle->transfer_last = 0;
    }
  }
  SDL_EndGPUCopyPass(copy_pass);
}

static void GPU_MemoryClearEntries()
{
  ForU32(bundle_index, GPU_MEMORY_TARGET_COUNT)
  {
    GPU_MemoryBundle *bundle = APP.gpu.mem.bundles + bundle_index;
    Assert(!bundle->transfer_first);
    Assert(!bundle->transfer_last);
    bundle->total_used = 0;
    bundle->element_count = 0;
  }
}

static void GPU_MemoryDeinit()
{
  ForU32(bundle_index, GPU_MEMORY_TARGET_COUNT)
  {
    GPU_MemoryBundle *bundle = APP.gpu.mem.bundles + bundle_index;
    if (bundle->buffer)
      SDL_ReleaseGPUBuffer(APP.gpu.device, bundle->buffer->handle);
  }
}