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
    append(sb, *buffer[0], to_align);
  }
}

PIE_AlignType :: (sb: *String_Builder, $type: Type)
{
  PIE_Align(sb, align_of(type));
}

PIE_ListStart :: (builder_owner: *String_Builder, list: *PIE_List, $type: Type)
{
  Check(!list.size);
  PIE_AlignType(builder_owner, type);
  list.offset = xx builder_string_length(builder_owner);
}

PIE_ListEnd :: (builder_owner: *String_Builder, list: *PIE_List)
{
  Check(!list.size);
  builder_length := builder_string_length(builder_owner);
  Check(builder_length >= list.offset);
  list.size = xx (builder_length - list.offset);
}
