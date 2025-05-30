#import "Basic";
#import "Bit_Operations";
#import "File";
#load "base_string.jai";
#load "base_arena.jai";
#load "base_hash.jai";
#load "base_vector.jai";
#load "base_math.jai";

// Range
Range :: struct ($T: Type)
{
  min, max: T;
};

Kilobyte :: v => v * 1024;
Megabyte :: v => v * 1024 * 1024;
Gigabyte :: v => v * 1024 * 1024 * 1024;

//
memclear :: (dst: *void, count: s64)
{
  memset(dst, 0, count);
}

IsInteger :: (T: Type) -> bool
{
  ti := cast(*Type_Info) T;
  return ti.type == .INTEGER;
}

CalculateMipMapCount :: (width: $T, height: T) -> T // @todo test
#modify { return IsInteger(T), "Argument is not of an integer type."; }
{
  max_dim := max(width, height);
  msb_plus_one := bit_scan_reverse(max_dim);
  return cast(T) msb_plus_one;
}

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

align_of :: ($T: Type) -> int #expand
{
  return #run -> int {
    info := type_info(struct{ p: u8; t: T; });
    return info.members[1].offset_in_bytes.(int);
  };
}
