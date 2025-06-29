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

ElapsedTime :: (from: TimestampMS, to: TimestampMS) -> TimestampMS
{
  if from > to return 0;
  return to - from;
}

ElapsedTimeS :: (from: TimestampMS, to: TimestampMS) -> float
{
  if from > to return 0;
  return (to - from).(float) * 0.001;
}

ElapsedTime :: (from: TimestampMS, to_predefine := TimestampPredefine.FRAME) -> TimestampMS
{
  return ElapsedTime(from, GetTime(to_predefine));
}

ElapsedTimeS :: (from: TimestampMS, to_predefine := TimestampPredefine.FRAME) -> float
{
  return ElapsedTimeS(from, GetTime(to_predefine));
}
