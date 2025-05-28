PIE_LOAD_File :: () -> string
{
  return G.ast.pie.file;
}

PIE_LOAD_Check :: (expr: bool, msg := "") -> bool
{
  if !expr
  {
    if !G.ast.pie.err
      log_error("Encountered ERROR when loading data from .PIE file. %\n", msg);
    G.ast.pie.err = true;
  }
  return expr;
}

//
// Common part
//
PIE_LOAD_OffsetSizeToString :: (offset: s64, size: s64) -> string
{
  result := STR_Substring(PIE_LOAD_File(), offset, offset+size);
  PIE_LOAD_Check(result.count == size);
  return result;
}
PIE_LOAD_ListToString :: (list: PIE_List) -> string
{
  return PIE_LOAD_OffsetSizeToString(list.offset, list.size);
}

PIE_LOAD_ListToArray :: (list: PIE_List, $T: Type) -> [] T
{
  T_align := align_of(T);
  PIE_LOAD_Check(list.offset % T_align == 0, #run tprint("List offset is unaligned for type %.", T));
  PIE_LOAD_Check(list.size % list.count == 0,
    tprint("List size (%) isn't divisible by list count (%) for type %.", list.size, list.count, T));
  PIE_LOAD_Check(list.size / list.count == size_of(T),
    tprint("List size (%) / list count (%) != size of % (%)", list.size, list.count, T, size_of(T)));

  pie_string := PIE_LOAD_ListToString(list);
  res: [] T;
  res.data = cast(*T) pie_string.data;
  res.count = list.count;
  return res;
}
