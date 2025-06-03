TICK_RATE :: 100;

TICK_PlayerInput :: struct
{
  move_dir: V2;
  // action buttons etc will be added here
  is_pathing: bool;
  pathing_world_p: V2;
};

TICK_Iterate :: ()
{
  // @impl
}

TICK_NormalizeInput:: (input: TICK_PlayerInput) -> TICK_PlayerInput
{
  res := input;
  for * res.move_dir.component
    if isnan(it.*)  it.* = 0.0;

  res.move_dir = normalize(res.move_dir);
  return res;
}
