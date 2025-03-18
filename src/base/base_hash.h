// Wrappers around xxhash.h
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"


static U64 U64_Hash(U64 seed, void *data, U64 size)
{
  return XXH3_64bits_withSeed(data, size, seed);
}

static U64 S8_Hash(U64 seed, S8 string)
{
  return U64_Hash(seed, string.str, string.size);
}
