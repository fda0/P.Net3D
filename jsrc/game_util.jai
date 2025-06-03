TickDeltas :: struct
{
  // This tracks things like every-increasing U64 ticks
  // last_tick == 0 is a special case that will trigger
  // init code
  last_tick: u64;
  deltas: [64] u16;
  next_delta_index: u16;
  tick_catchup: u16;
};

TickDeltas_AddTick :: (td: *TickDeltas, tick: u64) -> bool
{
  if td.last_tick == tick
    return false;

  first_init := !td.last_tick;
  td.last_tick = tick;

  if first_init
    return false;

  delta_u64 := ifx td.last_tick > tick then 0 else tick - td.last_tick;
  delta := CastSaturate(u16, delta_u64);

  index := td.next_delta_index;
  td.next_delta_index = (td.next_delta_index + 1) % td.deltas.count;

  td.deltas[index] = delta;
  return true;
}

TickDeltas_UpdateCatchup :: (td: *TickDeltas, current_delta: u16)
{
  if (!td.last_tick)
  return;

  max_delta: u16;
  for td.deltas
    max_delta = max(max_delta, it);

  target_delta := max_delta + (max_delta / 16) + 4;
  if current_delta > target_delta
    td.tick_catchup = current_delta - target_delta;
  else
    td.tick_catchup = 0;
}


Q_Push :: (arr: [] $T, range: *Range(u64)) -> *T
{
  assert(range.min <= range.max);
  current_index := range.max % arr.count.(u64);

  // truncate min if it overlaps with current
  if range.min != range.max // check that queue is not empty
  {
    min_index := range.min % arr.count.(u64);
    if min_index == current_index
    {
      range.min += 1;
    }
  }

  // advance max
  range.max += 1;

  res := *arr[current_index];
  return res;
}

Q_Peek :: (arr: [] $T, range: *Range(u64)) -> *T
{
  assert(range.min <= range.max);
  if range.min >= range.max
    return null;

  current_index := range.min % arr.count.(u64);
  res := *arr[current_index];
  return res;
}

Q_Pop :: (arr: [] $T, range: *Range(u64)) -> *T
{
  res := Q_Peek(arr, range);

  // advance min
  if (res)
    range.min += 1;

  return res;
}

Q_PeekAt :: (arr: [] $T, range: *Range(u64), peek_index: u64) -> * T
{
  assert(range.min <= range.max);
  if range.min >= range.max
    return 0;

  if !(peek_index >= range.min && peek_index < range.max)
    return 0;

  index := peek_index % arr.count;
  res := *arr[index];
  return res;
}



