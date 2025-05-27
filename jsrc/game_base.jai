#import "Basic";
#import "Math";
#import "xxHash";
#import "Flat_Pool";
#import "Bit_Operations";
#load "base_string.jai";

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

offset_of :: ($T: Type, member: string) -> int #compile_time {
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

// Flat_Pool
PoolAlloc :: (pool: *Flat_Pool, $T: Type, $init := false) -> *T
{
  data: *T = get(pool, size_of(T));
  #if init  Initialize(data);
  return data;
}

PoolAllocArray :: (pool: *Flat_Pool, $T: Type, count: s64, $init := false) -> [] T
{
  alloc_size := size_of(T)*count;
  data: *T = get(pool, alloc_size);

  #if init
  {
    ini :: initializer_of(T);
    if ini
      for 0..count-1  Initialize(*data[it]);
    else
      memclear(data, alloc_size);
  }

  array: [] T;
  array.count = count;
  array.data = data;
  return array;
}

//
memclear :: (dst: *void, count: s64)
{
  memset(dst, 0, count);
}

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

CalculateMipMapCount :: (width: $T, height: T) -> T // @todo test
#modify { return (cast(*Type_Info) T).type == .INTEGER, "Argument is not of an integer type."; }
{
  max_dim := max(width, height);
  msb := bit_scan_reverse(max_dim);
  if (msb == 0) return 0;

  T_bits := size_of(T)*8;
  return cast(T) T_bits - cast(T) msb;
}
#run print("CMMC debug: %\n", CalculateMipMapCount(1, 1));

IsPointerAligned :: (ptr: *void, alignment: s64) -> bool
{
  ptr_val := cast(s64) ptr;
  missaligned := ptr_val % alignment;
  return missaligned == 0;
}
