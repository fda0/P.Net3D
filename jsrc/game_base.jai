#import "Basic";
#import "Math";
#import "Bit_Operations";
#import "File";
#load "base_string.jai";
#load "base_arena.jai";
#load "base_hash.jai";

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

offset_of :: (ti: *Type_Info, member: string) -> int
{
  assert(ti.type == .STRUCT, "offset_of can be used on structs only");
  ts := cast(*Type_Info_Struct) ti;

  for ts.members {
    if it.name == member return it.offset_in_bytes;
  }
  assert(false, "Struct type info '%' does contain member '%'", ts, member);
  return -1;
}
offset_of :: ($T: Type, $member: string) -> int #expand
{
  return #run offset_of(type_info(T), member);
}

// align_of implementation by shwa
align_of :: (ti: *Type_Info) -> int
{
  if ti.type == {
    case .INTEGER;       return ti.runtime_size;
    case .FLOAT;         return ti.runtime_size;
    case .BOOL;          return ti.runtime_size;
    case .STRING;        return 8;
    case .POINTER;       return 8;
    case .PROCEDURE;     return 8;
    case .STRUCT;
    {
      dti := cast(*Type_Info_Struct)ti;
      m: int;
      for member: dti.members
      m = max(m, align_of(member.type));
      return m;
    }
    case .ARRAY;
    {
      dti := cast(*Type_Info_Array)ti;
      if dti.array_type ==
      {
        case .FIXED;     return align_of(dti.element_type);
        case .VIEW;      return 8;
        case .RESIZABLE; return 8;
        case; assert(false);
      }
    }
    case .ANY;           return 8;
    case .ENUM;          return ti.runtime_size;
    case .TYPE;          return ti.runtime_size;
    case .VARIANT;       return align_of((cast(*Type_Info_Variant)ti).variant_of);
    case; assert(false, "%", ti.type);
  }
  assert(false, "Unhandled align_of case - %", ti.*);
  return -1;
}
align_of :: ($T: Type) -> int #expand
{
  return #run align_of(type_info(T));
}
