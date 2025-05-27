#import "Basic";
#import "Math";
#import "xxHash";
#import "Flat_Pool";

V2 :: Vector2;
V3 :: Vector3;
V4 :: Vector4;
Quat :: #type,distinct Vector4;
Mat4 :: Matrix4;

// Range
Range :: struct ($T: Type)
{
  min, max: T;
};

Kilobyte :: v => v * 1024;
Megabyte :: v => v * 1024 * 1024;
Gigabyte :: v => v * 1024 * 1024 * 1024;

offset_of :: ($T: Type, $member: string) -> int {
  for type_info(T).members {
    if it.name == member return it.offset_in_bytes;
  }
  assert(false, "Type '%' does not have member '%'", T, member);
  return -1;
}

// One-shot, less optimal API
Hash64 :: (seed: u64, input: *void, size: u32) -> u64
{
  state: XXH64_state_t = ---;
  XXH64_reset(*state, seed);
  XXH64_update(*state, input, size);
  return XXH64_digest(*state);
}
Hash64 :: (seed: u64, input_str: string) -> u64
{
  return Hash64(seed, input_str.data, cast(u32) input_str.count);
}
Hash64T :: (seed: u64, input: $T) -> u64
{
  return Hash64(seed, *input, size_of(T));
}

// Init, Update, Digest API
Hash64_State :: XXH64_state_t;
Hash64_Init :: (seed: u64) -> Hash64_State
{
  state: Hash64_State = ---;
  XXH64_reset(*state, seed);
  return state;
}
Hash64_Update :: (state: *Hash64_State, input: *void, size: u32)
{
  XXH64_update(state, input, size);
}
Hash64_Update :: (state: *Hash64_State, input_str: string)
{
  XXH64_update(state, input_str.data, cast(u32) input_str.count);
}
Hash64_UpdateT :: (state: *Hash64_State, input: $T)
{
  XXH64_update(state, *input, size_of(T));
}
Hash64_Digest :: (state: *Hash64_State) -> u64
{
  return XXH64_digest(state);
}

//
CeilPow2 :: (v: u32) -> u32
{
  if (!v) return 0;
  v -= 1;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v += 1;
  return v;
}

CalculateMipMapCount :: (width: u32, height: u32) -> u32
{
  max_dim := max(width, height);
  msb := MostSignificantBitU32(max_dim);
  if (msb < 0) return 0;
  return msb;
}