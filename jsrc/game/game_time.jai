TimestampMS :: #type,distinct u64;

TimestampPredefine :: enum
{
  FRAME;
  NOW;
}

GetTime :: (predefine: TimestampPredefine) -> TimestampMS
{
  if #complete predefine == {
    case .FRAME; return G.frame_timestamp;
    case .NOW; return xx SDL_GetTicks();
  }
}

ElapsedTime :: (from: TimestampMS, to: TimestampMS) -> TimestampMS
{
  return to - from;
}

ElapsedTimeS :: (from: TimestampMS, to: TimestampMS) -> float
{
  return (to - from).(float) * 0.001;
}

ElapsedTime :: (from: TimestampMS, to_predefine: TimestampPredefine) -> TimestampMS
{
  return GetTime(to_predefine) - from;
}

ElapsedTimeS :: (from: TimestampMS, to_predefine: TimestampPredefine) -> float
{
  return (GetTime(to_predefine) - from).(float) * 0.001;
}
