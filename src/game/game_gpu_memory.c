/* @todo
- Do not free and allocate transfers and buffers all the time. Reuse them.
- Think about overlapping GPU work. Now everything happens at the end of the frame in the final command buffer.
*/

static bool GPU_MEM_TargetMatch(GPU_MEM_Target a, GPU_MEM_Target b)
{
  if (a.type == b.type)
  {
    if (a.type == GPU_MEM_MeshVertices)
      return MATERIAL_KeyMatch(a.material_key, b.material_key);

    if (a.type == GPU_MEM_ModelInstances)
      return MODEL_KeyMatch(a.model_key, b.model_key);
  }
  return false;
}

static GPU_MEM_Batch *GPU_MEM_FindBundle(GPU_MEM_Target target)
{
  GPU_MEM_Batch *batch = 0;
  ForU32(i, APP.gpu.mem.batches_count)
  {
    GPU_MEM_Batch *search = APP.gpu.mem.batches + i;
    if (GPU_MEM_TargetMatch(search->target, target))
    {
      batch = search;
      break;
    }
  }
  return batch;
}

static GPU_MEM_Batch *GPU_MEM_FindOrCreateBundle(GPU_MEM_Target target)
{
  GPU_MEM_Batch *batch = GPU_MEM_FindBundle(target);
  if (!batch)
  {
    Assert(APP.gpu.mem.batches_count < ArrayCount(APP.gpu.mem.batches));
    batch = APP.gpu.mem.batches + APP.gpu.mem.batches_count;
    APP.gpu.mem.batches_count += 1;
    // We don't need to clear batch since
    // batches array is zero initialized on game launch.
    // + APP.gpu.mem.batches_count isn't reset between frames.
    batch->target = target;
  }
  return batch;
}

static void GPU_MEM_TransferUnmap(GPU_MEM_Transfer *transfer)
{
  Assert(transfer->mapped_memory);
  SDL_UnmapGPUTransferBuffer(APP.gpu.device, transfer->handle);
  transfer->mapped_memory = 0;
}

static GPU_MEM_Transfer *GPU_MEM_TransferCreate(GPU_MEM_Batch *batch, U32 size)
{
  // Calculate rounded-up alloc_size
  U32 alloc_size = U32_CeilPow2(size*2);
  if (batch->buffer)
    alloc_size = Max(alloc_size, batch->buffer->cap);

  // Reclaim GPU_MEM_TransferStorage from free list or allocate a new one
  // Zero initialize it
  GPU_MEM_Transfer *result = APP.gpu.mem.free_transfers;
  {
    if (result)
      APP.gpu.mem.free_transfers = APP.gpu.mem.free_transfers->next;
    else
      result = Alloc(APP.gpu.mem.arena, GPU_MEM_Transfer, 1);

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

  // Chain result into its GPU_MEM_Entry
  {
    if (batch->transfer_last)
      batch->transfer_last->next = result;

    batch->transfer_last = result;

    if (!batch->transfer_first)
      batch->transfer_first = result;
  }

  return result;
}

static void *GPU_MEM_TransferGetMappedMemory(GPU_MEM_Batch *batch, U32 size, U32 elem_count)
{
  batch->element_count += elem_count;

  GPU_MEM_Transfer *transfer = batch->transfer_last;

  if (transfer)
  {
    Assert(transfer->cap > transfer->used);
    if (transfer->cap - transfer->used < size)
    {
      GPU_MEM_TransferUnmap(transfer);
      transfer = 0;
    }
  }

  if (!transfer)
    transfer = GPU_MEM_TransferCreate(batch, size);

  void *result = (U8 *)transfer->mapped_memory + transfer->used;
  batch->total_used += size;
  transfer->used += size;
  return result;
}

static void GPU_MEM_TransferUploadBytes(GPU_MEM_Batch *batch, void *data, U32 size, U32 elem_count)
{
  void *dest = GPU_MEM_TransferGetMappedMemory(batch, size, elem_count);
  Memcpy(dest, data, size);
}

//
static GPU_MEM_Buffer *GPU_MEM_AllocBuffer(U32 alloc_size, SDL_GPUBufferUsageFlags usage)
{
  GPU_MEM_Buffer *buffer = APP.gpu.mem.free_buffers;
  {
    if (buffer)
      APP.gpu.mem.free_buffers = APP.gpu.mem.free_buffers->next;
    else
      buffer = Alloc(APP.gpu.mem.arena, GPU_MEM_Buffer, 1);

    Memclear(buffer, sizeof(*buffer));
  }

  SDL_GPUBufferCreateInfo buffer_info = {.usage = usage, .size = alloc_size};
  buffer->handle = SDL_CreateGPUBuffer(APP.gpu.device, &buffer_info);
  buffer->cap = alloc_size;
  return buffer;
}

static void GPU_MEM_UploadBatch(SDL_GPUCopyPass *copy_pass, GPU_MEM_Batch *batch)
{
  if (batch->total_used)
  {
    // Check if current buffer is big enough.
    // Free it if it's too small.
    if (batch->buffer)
    {
      if (batch->buffer->cap < batch->total_used)
      {
        SDL_ReleaseGPUBuffer(APP.gpu.device, batch->buffer->handle);

        // Move BufferStorage to free list
        batch->buffer->next = APP.gpu.mem.free_buffers;
        APP.gpu.mem.free_buffers = batch->buffer;
        batch->buffer = 0;
      }
    }

    // Allocate buffer
    if (!batch->buffer)
    {
      SDL_GPUBufferUsageFlags usage = 0;
      if (batch->target.type == GPU_MEM_MeshVertices)
        usage |= SDL_GPU_BUFFERUSAGE_VERTEX;
      else
        usage |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;

      batch->buffer = GPU_MEM_AllocBuffer(U32_CeilPow2(batch->total_used*2), usage);
    }

    // Transfers -> Buffer
    U32 offset = 0;
    for (GPU_MEM_Transfer *transfer = batch->transfer_first;
         transfer;
         transfer = transfer->next)
    {
      SDL_GPUTransferBufferLocation source = { .transfer_buffer = transfer->handle };
      SDL_GPUBufferRegion destination =
      {
        .buffer = batch->buffer->handle,
        .offset = offset,
        .size = transfer->used
      };
      SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);

      offset += transfer->used;
    }

    // Free transfers
    GPU_MEM_Transfer *transfer = batch->transfer_first;
    while (transfer)
    {
      GPU_MEM_Transfer *next = transfer->next;

      SDL_ReleaseGPUTransferBuffer(APP.gpu.device, transfer->handle);
      transfer->next = APP.gpu.mem.free_transfers;
      APP.gpu.mem.free_transfers = transfer;

      transfer = next;
    }

    // clear batch transfer links
    batch->transfer_first = 0;
    batch->transfer_last = 0;
  }
}

static void GPU_MEM_UploadAllBatches(SDL_GPUCommandBuffer *cmd)
{
  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

  ForU32(batch_index, APP.gpu.mem.batches_count)
  {
    GPU_MEM_Batch *batch = APP.gpu.mem.batches + batch_index;
    GPU_MEM_UploadBatch(copy_pass, batch);
  }
  GPU_MEM_UploadBatch(copy_pass, &APP.gpu.mem.poses);

  SDL_EndGPUCopyPass(copy_pass);
}

static void GPU_MEM_PostFrame()
{
  ForU32(batch_index, APP.gpu.mem.batches_count)
  {
    GPU_MEM_Batch *batch = APP.gpu.mem.batches + batch_index;
    Assert(!batch->transfer_first);
    Assert(!batch->transfer_last);
    batch->total_used = 0;
    batch->element_count = 0;
  }
  APP.gpu.mem.poses.total_used = 0;
  APP.gpu.mem.poses.element_count = 0;
}

static void GPU_MEM_Init()
{
  APP.gpu.mem.poses.buffer = GPU_MEM_AllocBuffer(16, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
}

static void GPU_MEM_Deinit()
{
  ForU32(batch_index, APP.gpu.mem.batches_count)
  {
    GPU_MEM_Batch *batch = APP.gpu.mem.batches + batch_index;
    if (batch->buffer)
      SDL_ReleaseGPUBuffer(APP.gpu.device, batch->buffer->handle);
  }
}