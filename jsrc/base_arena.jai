// Bump allocator wrapper - that bundles pool & allocator structs.
Arena :: struct
{
  pool: Flat_Pool;
  #as using alloc: Allocator;
};

ARENA_DEFAULT_VIRTUAL_MEMORY_RESERVE :: 256 * 1024 * 1024;
#import "Flat_Pool";

init :: (arena: *Arena, reserve := ARENA_DEFAULT_VIRTUAL_MEMORY_RESERVE)
{
  init(*arena.pool, reserve);
  arena.alloc.proc = flat_pool_allocator_proc;
  arena.alloc.data = *arena.pool;
}
