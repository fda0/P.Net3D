static void TEST_GPU()
{
  // GPU_MemoryIndexToSize
  Assert(GPU_MemoryIndexToSize(0) == 16);
  Assert(GPU_MemoryIndexToSize(1) == 64);
  Assert(GPU_MemoryIndexToSize(2) == 256);
  Assert(GPU_MemoryIndexToSize(3) == 1024);
  // GPU_MemorySizeToIndex
  Assert(GPU_MemorySizeToIndex(1023) == 3);
  Assert(GPU_MemorySizeToIndex(1024) == 3);
  Assert(GPU_MemorySizeToIndex(1025) == 4);
  Assert(GPU_MemorySizeToIndex(0) == 0);
  Assert(GPU_MemorySizeToIndex(1) == 0);
  Assert(GPU_MemorySizeToIndex(16) == 0);
  Assert(GPU_MemorySizeToIndex(17) == 1);
  Assert(GPU_MemorySizeToIndex(63) == 1);
  Assert(GPU_MemorySizeToIndex(64) == 1);
  Assert(GPU_MemorySizeToIndex(65) == 2);
}

static void TEST_RunOnce()
{
  TEST_GPU();
}
