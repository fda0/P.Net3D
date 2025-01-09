#if defined(__clang__)
# if __has_feature(address_sanitizer)
#  include "sanitizer/asan_interface.h"
#  define ASAN_MARK_POISON(ptr, size) __asan_poison_memory_region(ptr, size)
#  define ASAN_MARK_UNPOISON(ptr, size) __asan_unpoison_memory_region(ptr, size)
# endif
#endif
#ifndef ASAN_MARK_POISON
# define ASAN_MARK_POISON(ptr, size)
# define ASAN_MARK_UNPOISON(ptr, size)
#endif

#ifdef __clang__ // WARNIGNS BOILERPLATE START
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
#endif

typedef struct Arena Arena;
struct Arena {
    U8 *base;
    U64 used;
    U64 capacity;
};

// ---
// Allocate
// ---
static U8 *Arena_AllocateBytes(Arena *a, U64 bytes_to_allocate, U64 alignment, bool zero_init)
{
    U64 align_reminder = ModuloPow2(a->used, alignment);
    if (align_reminder) {
        U64 bytes_that_have_to_be_wasted = alignment - align_reminder;
        a->used += bytes_that_have_to_be_wasted;
    }

    if ((a->used + bytes_to_allocate) > a->capacity)
    {
        // Arena capacity overflow!
        // @todo allocate more memory; grow capacity if possible?
        Assert(false);
        return 0;
    }

    U8 *result = a->base + a->used;
    a->used += bytes_to_allocate;

    ASAN_MARK_UNPOISON(result, bytes_to_allocate); // align padding should remain poisoned
    if (zero_init) {
        memset(result, 0, bytes_to_allocate);
    }
    return result;
}

#define Allocate(ARENA, TYPE, COUNT) Arena_AllocateBytes(ARENA, sizeof(TYPE)*COUNT, _Alignof(TYPE), false)
#define AllocateZero(ARENA, TYPE, COUNT) Arena_AllocateBytes(ARENA, sizeof(TYPE)*COUNT, _Alignof(TYPE), true)

// ---
// Reset
// ---
static void Arena_Reset(Arena *a, U64 new_used)
{
    Assert(a->capacity >= new_used);
    Assert(a->used >= new_used);
    a->used = new_used;
    ASAN_MARK_POISON(a->base + a->used, a->capacity - a->used);
}

// ---
// Query
// ---
static bool Arena_IsThisYours(Arena *a, void *some_ptr)
{
    U64 base = (U64)a->base;
    U64 end = (base + a->capacity);
    U64 ptr = (U64)some_ptr;
    return (base >= ptr && ptr < end);
}

// ---
// Make
// ---
static Arena *Arena_MakeInside(void *memory, U64 memory_size)
{
    Assert(memory_size > sizeof(Arena));
    Assert(((U64)memory % _Alignof(Arena)) == 0);
    if (!memory) {
        return 0;
    }

    Arena *result = (Arena *)memory;
    *result = (Arena){0};
    result->base = (U8 *)memory + sizeof(Arena);
    result->capacity = memory_size - sizeof(Arena);
    ASAN_MARK_POISON(result->base + sizeof(Arena), result->capacity - sizeof(Arena));
    return result;
}

static Arena Arena_MakeOutside(void *memory, U64 memory_size)
{
    Arena result = {};
    result.base = (U8 *)memory;
    result.capacity = memory_size;
    ASAN_MARK_POISON(result.base, result.capacity);
    return result;
}

static Arena *Arena_MakeSubArena(Arena *a, U64 sub_capacity)
{
    U64 total_size = sizeof(Arena) + sub_capacity;
    void *arena_memory = Arena_AllocateBytes(a, total_size, _Alignof(Arena), true);
    Arena *result = Arena_MakeInside(arena_memory, total_size);
    Assert(result->capacity == sub_capacity);
    return result;
}

#ifdef __clang__ // WARNINGS BOILERPLATE END
#pragma clang diagnostic pop
#endif
