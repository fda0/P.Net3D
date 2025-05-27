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
