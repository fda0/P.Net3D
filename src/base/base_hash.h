// Wrappers around meow_hash
#include "meow_hash_x64_aesni.h"
typedef meow_state HashState;

static U64 U64_Hash(void *data, U64 size)
{
  meow_u128 hash = MeowHash(MeowDefaultSeed, size, data);
  U64 res = MeowU64From(hash, 0);
  return res;
}

static U64 S8_Hash(S8 string)
{
  return U64_Hash(string.str, string.size);
}

static HashState HashBegin()
{
  HashState state;
  MeowBegin(&state, MeowDefaultSeed);
  return state;
}

static void U64_HashAbsorb(HashState *state, void *data, U64 size)
{
  MeowAbsorb(state, size, data);
}

static void S8_HashAbsorb(HashState *state, S8 string)
{
  MeowAbsorb(state, string.size, string.str);
}

static U64 HashEnd(HashState *state)
{
  meow_u128 hash = MeowEnd(state, 0);
  U64 res = MeowU64From(hash, 0);
  return res;
}

#if 0 // old xxhash version
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
#endif
