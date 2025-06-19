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

PIE_Init :: ()
{
  using B.pie;
  remember_allocators(*file);
  remember_allocators(*vertices);
  remember_allocators(*indices);
  remember_allocators(*models);
  remember_allocators(*skeletons);
  remember_allocators(*materials);
}

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

PIE_AlignType :: (sb: *String_Builder, $type: Type)
{
  PIE_Align(sb, align_of(type));
}


// @todo imporve the api so the usage looks more like:
// list_ctx := PIE_ListStart(PIE.b.file, some_list); // is something other than PIE.b.file ever used? shoudl that be implicit?
// PIE_ListAdd(list_ctx, some_data); // This does typechecking between some_data and some_list types (could be done statically!).
// PIE_ListEnd(list_ctx);
//
// rename PIE_ListAddArray -> PIE_ListArray
//

PIE_ListStart :: (sb: *String_Builder, list: *PIE_ListT($list_type), $type: Type)
{
  #assert(list_type == type);
  Check(!list.size);
  PIE_AlignType(sb, type);
  list.offset = xx builder_string_length(sb);
}

PIE_ListEnd :: (sb: *String_Builder, list: *PIE_List)
{
  Check(!list.size);
  builder_length := builder_string_length(sb);
  Check(builder_length >= list.offset);
  list.size = xx (builder_length - list.offset);
}

PIE_ListAppend :: (sb: *String_Builder, value: $T)
{
  append(sb, (*value).(*u8), size_of(T));
}

PIE_ListAddArray :: (sb: *String_Builder, list: *PIE_ListT($list_type), array: [] $T)
{
  #assert(list_type == T);
  PIE_ListStart(sb, list, T);
  append(sb, array.data.(*u8), size_of(T) * array.count);
  PIE_ListEnd(sb, list);
}
