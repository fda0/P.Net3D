TICK_RATE :: 100;

TICK_Input :: struct
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
