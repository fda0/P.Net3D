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

  if (index < MDL_COUNT)
  {
    target.type = GPU_MemoryModelInstances;
    target.model = index;
    return target;
  }
  index -= MDL_COUNT;

  Assert(index == 0);
  target.type = GPU_MemoryJointTransforms;
  return target;
}

static U32 GPU_MemoryTargetToIndex(GPU_MemoryTarget target)
{
  U32 index = 0;
  if (target.type == GPU_MemoryMeshVertices)
    return index + target.tex;
  index += TEX_COUNT;

  if (target.type == GPU_MemoryModelInstances)
    return index + target.model;
  index += MDL_COUNT;

  Assert(target.type == GPU_MemoryJointTransforms);
  return index;
}

static GPU_MemoryEntry *GPU_MemoryTargetToEntry(GPU_MemoryTarget target)
{
  U32 entry_index = GPU_MemoryTargetToIndex(target);
  GPU_MemoryEntry *entry = APP.gpu.mem.entries + entry_index;
  return entry;
}

static void GPU_MemoryTransferUnmap(GPU_TransferStorage *storage)
{
  Assert(storage->mapped_memory);
  SDL_UnmapGPUTransferBuffer(APP.gpu.device, storage->handle);
  storage->mapped_memory = 0;
}

static GPU_TransferStorage *GPU_MemoryTransferCreate(GPU_MemoryTarget target, U32 size)
{
  GPU_MemoryEntry *entry = GPU_MemoryTargetToEntry(target);

  // Calculate rounded-up alloc_size
  U32 alloc_size = U32_CeilPow2(size*2);
  if (entry->buffer)
    alloc_size = Max(alloc_size, entry->buffer->cap);

  // Reclaim GPU_TransferStorage from free list or allocate a new one
  // Zero initialize it
  GPU_TransferStorage *result = APP.gpu.mem.free_transfers;
  {
    if (result)
      APP.gpu.mem.free_transfers = APP.gpu.mem.free_transfers->next;
    else
      result = Alloc(APP.gpu.mem.arena, GPU_TransferStorage, 1);

    Memclear(result, sizeof(*result));
  }

  // GPU alloc new transfer buffer and map it
  SDL_GPUTransferBufferCreateInfo transfer_info =
  {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = alloc_size
  };
  result->handle = SDL_CreateGPUTransferBuffer(APP.gpu.device, &transfer_info);
  result->mapped_memory = SDL_MapGPUTransferBuffer(APP.gpu.device, result->handle, false);

  // Chain result into its GPU_MemoryEntry
  {
    if (entry->transfer_last)
      entry->transfer_last->next = result;

    entry->transfer_last = result;

    if (!entry->transfer_first)
      entry->transfer_first = result;
  }

  return result;
}

static void *GPU_MemoryTransferMappedMemory(GPU_MemoryTarget target, U32 size, U32 elem_count)
{
  GPU_MemoryEntry *entry = GPU_MemoryTargetToEntry(target);
  entry->element_count += elem_count;

  GPU_TransferStorage *storage = entry->transfer_last;

  if (storage && (storage->cap - storage->used < size))
  {
    GPU_MemoryTransferUnmap(storage);
    storage = 0;
  }

  if (!storage)
    storage = GPU_MemoryTransferCreate(target, size);

  void *result = (U8 *)storage->mapped_memory + storage->used;
  entry->total_used += size;
  storage->used += size;
  return result;
}

static void GPU_MemoryTransferUploadBytes(GPU_MemoryTarget target, void *data, U32 size, U32 elem_count)
{
  void *dest = GPU_MemoryTransferMappedMemory(target, size, elem_count);
  Memcpy(dest, data, size);
}

//
static void GPU_MemoryTransferToBuffers(SDL_GPUCommandBuffer *cmd)
{
  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
  ForU32(entry_index, GPU_MEMORY_TARGET_COUNT)
  {
    GPU_MemoryEntry *entry = APP.gpu.mem.entries + entry_index;
    if (entry->total_used)
    {
      // Check if current buffer is big enough.
      // Free it if it's too small.
      if (entry->buffer)
      {
        if (entry->buffer->cap < entry->total_used)
        {
          SDL_ReleaseGPUBuffer(APP.gpu.device, entry->buffer->handle);

          // Move BufferStorage to free list
          entry->buffer->next = APP.gpu.mem.free_buffers;
          APP.gpu.mem.free_buffers = entry->buffer;
          entry->buffer = 0;
        }
      }

      // Allocate buffer
      if (!entry->buffer)
      {
        entry->buffer = APP.gpu.mem.free_buffers;
        {
          if (entry->buffer)
            APP.gpu.mem.free_buffers = APP.gpu.mem.free_buffers->next;
          else
            entry->buffer = Alloc(APP.gpu.mem.arena, GPU_BufferStorage, 1);

          Memclear(entry->buffer, sizeof(*entry->buffer));
        }

        U32 alloc_size = U32_CeilPow2(entry->total_used*2);

        GPU_MemoryTargetType target_type = GPU_MemoryIndexToTarget(entry_index).type;
        SDL_GPUBufferUsageFlags usage = 0;
        if (target_type == GPU_MemoryMeshVertices)
          usage |= SDL_GPU_BUFFERUSAGE_VERTEX;
        else
          usage |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;

        entry->buffer->cap = alloc_size;
        SDL_GPUBufferCreateInfo buffer_info = {.usage = usage, .size = alloc_size};
        entry->buffer->handle = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_info);
      }

      // Transfers -> Buffer
      U32 offset = 0;
      for (GPU_TransferStorage *transfer_storage = entry->transfer_first;
           transfer_storage;
           transfer_storage = transfer_storage->next)
      {
        SDL_GPUTransferBufferLocation source = { .transfer_buffer = transfer_storage->handle };
        SDL_GPUBufferRegion destination =
        {
          .buffer = entry->buffer->handle,
          .offset = offset,
          .size = transfer_storage->used
        };
        SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);

        offset += transfer_storage->used;
      }

      // Free transfers
      GPU_TransferStorage *transfer_storage = entry->transfer_first;
      while (transfer_storage)
      {
        GPU_TransferStorage *next = transfer_storage->next;

        SDL_ReleaseGPUTransferBuffer(APP.gpu.device, transfer_storage->handle);
        transfer_storage->next = APP.gpu.mem.free_transfers;
        APP.gpu.mem.free_transfers = transfer_storage;

        transfer_storage = next;
      }

      // clear entry transfer links
      entry->transfer_first = 0;
      entry->transfer_last = 0;
    }
  }
  SDL_EndGPUCopyPass(copy_pass);
}

static void GPU_MemoryClearEntries()
{
  ForU32(entry_index, GPU_MEMORY_TARGET_COUNT)
  {
    GPU_MemoryEntry *entry = APP.gpu.mem.entries + entry_index;
    Assert(!entry->transfer_first);
    Assert(!entry->transfer_last);
    entry->total_used = 0;
    entry->element_count = 0;
  }
}
