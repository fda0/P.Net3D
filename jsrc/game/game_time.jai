TimestampMS :: #type,distinct u64;

TimestampPredefine :: enum
{
  FRAME;
  NOW;
}

GetTime :: (predefine := TimestampPredefine.FRAME) -> TimestampMS
{
  if #complete predefine == {
    case .FRAME; return G.frame_timestamp;
    case .NOW; return xx SDL_GetTicks();
  }
}

ElapsedTime :: (from: TimestampMS, to: TimestampMS, max_on_fail := false) -> TimestampMS
{
  if from > to return cast(TimestampMS) ifx max_on_fail then ~0 else 0;
  return to - from;
}

ElapsedTimeS :: (from: TimestampMS, to: TimestampMS, max_on_fail := false) -> float
{
  if from > to return ifx max_on_fail then FLOAT32_MAX else 0;
  if (to - from) > FLOAT32_MAX.(u64) return FLOAT32_MAX;
  return (to - from).(float) * 0.001;
}

ElapsedTime :: (from: TimestampMS, to_predefine := TimestampPredefine.FRAME, max_on_fail := false) -> TimestampMS
{
  return ElapsedTime(from, GetTime(to_predefine), max_on_fail);
}

ElapsedTimeS :: (from: TimestampMS, to_predefine := TimestampPredefine.FRAME, max_on_fail := false) -> float
{
  return ElapsedTimeS(from, GetTime(to_predefine), max_on_fail);
}
