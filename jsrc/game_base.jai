#import "Basic";
#import "Math";
#import "xxHash";
V2 :: Vector2;
V3 :: Vector3;
V4 :: Vector4;
Mat4 :: Matrix4;

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

size_of_type :: T => size_of(type_of(T));
