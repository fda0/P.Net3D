PIE_Builder :: struct
{
  file: Flat_Pool;
  finalized: bool;

  active_list: *PIE_List;
  active_list_type: Type;

  vertices: [..] WORLD_Vertex;
  indices: [..] u16;

  models: [..] PIE_Model;
  skeletons: [..] PIE_Skeleton;
  materials: [..] PIE_Material;
};

PIE_Init :: ()
{
  using B.pie;
  file.alignment = 1; // We are doing our own, tightly packed, precise alignment on per-type basis.
  init(*file, reserve = Gigabyte(2));
  remember_allocators(*vertices);
  remember_allocators(*indices);
  remember_allocators(*models);
  remember_allocators(*skeletons);
  remember_allocators(*materials);
}

PIE_CurrentFileOffset :: () -> u32
{
  using B.pie;
  offset := file.current_point.(u64) - file.memory_base.(u64);
  return xx offset;
}

// Alignment
PIE_AlignFile :: (alignment: s64)
{
  using B.pie;

  current_offset := PIE_CurrentFileOffset();
  missaligned_bytes := current_offset % alignment;
  if missaligned_bytes
  {
    to_align := alignment - missaligned_bytes;
    align_bytes_memory := get(*file, to_align);
    memset(align_bytes_memory, 0, to_align);
  }
}

PIE_AlignFileToType :: ($T: Type)
{
  PIE_AlignFile(align_of(T));
}

PIE_FileIsAlignedToType :: ($T: Type) -> bool
{
  current_offset := PIE_CurrentFileOffset();
  alignment := align_of(T);
  missaligned_bytes := current_offset % alignment;
  return missaligned_bytes == 0;
}

// ListStart / ListEnd
PIE_ListStart :: (list: *PIE_ListT($LT))
{
  using B.pie;
  Check(!list.size);
  Check(!active_list && !active_list_type);
  active_list = list;
  active_list_type = LT;

  PIE_AlignFileToType(LT);
  list.offset = PIE_CurrentFileOffset();
}

PIE_ListEnd :: ()
{
  using B.pie;
  Check(active_list && active_list_type);
  Check(!active_list.size);

  current_offset := PIE_CurrentFileOffset();
  Check(current_offset >= active_list.offset);
  active_list.size = current_offset - active_list.offset;

  active_list_type_size := active_list_type.(*Type_Info).runtime_size;
  Check(active_list.size % active_list_type_size == 0);

  active_list = null;
  memset(*active_list_type, 0, size_of(Type));
}

// AllocInFile
PIE_AllocBytesInFile :: (size: s64) -> *void
{
  return get(*B.pie.file, size);
}

PIE_AllocTypeInFile :: ($T: Type) -> *T
{
  using B.pie;
  Check(active_list_type == T);
  Check(PIE_FileIsAlignedToType(T));
  return PIE_AllocBytesInFile(size_of(T));
}

PIE_AllocArrayInFile :: (count: s64, $T: Type) -> [] T
{
  using B.pie;
  Check(active_list_type == T);
  Check(PIE_FileIsAlignedToType(T));

  result: [] T;
  result.count = count;
  result.data = PIE_AllocBytesInFile(count * size_of(T));
  return result;
}

// FileAppend
PIE_FileAppendValue :: (value: $T)
{
  dst := PIE_AllocTypeInFile(T);
  dst.* = value;
}

PIE_FileAppendArray :: (array: [] $T)
{
  dst := PIE_AllocArrayInFile(array.count, T);
  memcpy(dst.data, array.data, size_of(T)*array.count);
}

PIE_FileAppendString :: (str: string)
{
  using B.pie;
  Check(active_list_type == u8);
  PIE_FileAppendArray(cast([] u8) str);
}

PIE_FileAppendPrint :: (format_string: string, args: .. Any)
{
  auto_release_temp();
  printed := tprint(format_string, ..args);
  PIE_FileAppendString(printed);
}

// ListSet
PIE_ListSetArray :: (list: *PIE_ListT($LT), array: [] $T)
{
  PIE_ListStart(list);
  PIE_FileAppendArray(array);
  PIE_ListEnd();
}

PIE_ListSetString :: (list: *PIE_ListT($LT), str: string)
{
  PIE_ListStart(list);
  PIE_FileAppendString(str);
  PIE_ListEnd();
}
