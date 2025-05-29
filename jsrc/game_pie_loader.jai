PIE_LOAD_File :: () -> string
{
  return G.ast.pie.file;
}

PIE_LOAD_Check :: (expr: bool, msg: string, critical := true) -> bool
{
  if !expr
  {
    if critical
    {
      // @todo Create a popup window error when asset file failed to load.
      if !G.ast.pie.err
        log_error(".PIE CRITICAL ERROR: %", msg);
      G.ast.pie.err = true;
    }
    else
      log_error(".PIE ERROR: %", msg);
  }
  return expr;
}

//
// Common part
//
PIE_LOAD_OffsetSizeToString :: (offset: s64, size: s64) -> string
{
  result := STR_Substring(PIE_LOAD_File(), offset, offset+size);
  PIE_LOAD_Check(result.count == size,
    tprint("Out of bounds access. Offset: %, size: %. File size: %.", offset, size, PIE_LOAD_File().count));
  return result;
}

PIE_LOAD_OffsetSizeToArray :: (offset: s64, size: s64, $T: Type) -> [] T
{
  PIE_LOAD_Check(size > 0, tprint("Size (%) has to be > 0", size));

  T_align := align_of(T);
  T_size := size_of(T);
  PIE_LOAD_Check(offset % T_align == 0, tprint("Offset (%) is unaligned for type % (align=%).", offset, T, T_align));

  count := size / T_size;
  PIE_LOAD_Check(size % T_size == 0, tprint("Size (%) isn't divisible by size of type % (size=%).", size, T_size, T));

  pie_string := PIE_LOAD_OffsetSizeToString(offset, size);
  res: [] T;
  res.data = cast(*T) pie_string.data;
  res.count = count;
  return res;
}

PIE_LOAD_ListToString :: (list: PIE_List) -> string
{
  return PIE_LOAD_OffsetSizeToString(list.offset, list.size);
}

PIE_LOAD_ListToArray :: (list: PIE_List, $T: Type) -> [] T
{
  return PIE_LOAD_OffsetSizeToArray(list.offset, list.size, T);
}
