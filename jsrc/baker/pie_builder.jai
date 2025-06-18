PIE_Builder :: struct
{
  file: String_Builder;
  finalized: bool;

  vertices: [..] WORLD_Vertex;
  indices: [..] u16;

  models: [..] PIE_Model;
  skeletons: [..] PIE_Skeleton;
  materials: [..] PIE_Material;
};

PIE_Align :: (sb: *String_Builder, alignment: s64)
{
  sb_length := builder_string_length(sb);
  missaligned_bytes := sb_length % alignment;
  if missaligned_bytes
  {
    to_align := alignment - missaligned_bytes;
    buffer: [256] u8; // array with zeroes
    assert(to_align <= buffer.count);
    append(sb, buffer.data, to_align);
  }
}

PIE_AlignType :: (sb_owner: *String_Builder, $type: Type)
{
  PIE_Align(sb_owner, align_of(type));
}

PIE_ListStart :: (sb_owner: *String_Builder, list: *PIE_List, $type: Type)
{
  Check(!list.size);
  PIE_AlignType(sb_owner, type);
  list.offset = xx builder_string_length(sb_owner);
}

PIE_ListEnd :: (sb_owner: *String_Builder, list: *PIE_List)
{
  Check(!list.size);
  builder_length := builder_string_length(sb_owner);
  Check(builder_length >= list.offset);
  list.size = xx (builder_length - list.offset);
}

PIE_ListAddArray :: (sb_owner: *String_Builder, list: *PIE_List, array: [] $T)
{
  PIE_ListStart(sb_owner, list, T);
  append(sb_owner, array.data.(*u8), size_of(T) * array.count);
  PIE_ListEnd(sb_owner, list);
}
