STR_MatchFlags :: enum_flags u32
{
  FindLast;
  RightSideSloppy;
  CaseInsensitive;
  SlashInsensitive;
};

STR_Range :: (first: *u8, last: *u8) -> string
{
  res: string;
  res.data = first;
  res.count = cast(s64)last - cast(s64)first;
  return res;
}
STR_Substring :: (str: string, sub_min: s64, sub_max: s64) -> string
{
  sub_max = clamp(sub_max, 0, str.count);
  sub_min = clamp(sub_min, 0, str.count);
  if sub_min > sub_max  Swap(*sub_min, *sub_max);

  res: string;
  res.count = sub_max - sub_min;
  res.data = str.data + sub_min;
  return res;
}
STR_Skip :: (str: string, distance: s64) -> string
{
  distance = clamp(distance, 0, str.count);

  res := str;
  res.data += distance;
  res.count -= distance;
  return res;
}
S8_Chop :: (str: string, distance_from_end: s64) -> string
{
  distance_from_end = clamp(distance_from_end, 0, str.count);

  res := str;
  res.count -= distance_from_end;
  return res;
}
S8_Prefix :: (str: string, count: s64) -> string
{
  count = clamp(count, 0, str.count);

  res := str;
  res.count = count;
  return res;
}
S8_Suffix :: (str: string, count: s64) -> string
{
  count = clamp(count, 0, str.count);
  distance := str.count - count;

  res := str;
  res.data += distance;
  res.count = count;
  return res;
}
