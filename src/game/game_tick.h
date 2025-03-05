typedef struct
{
  V2 move_dir;
  // action buttons etc will be added here
  bool is_pathing;
  V2 pathing_world_p;
} TICK_Input;

static TICK_Input TICK_NormalizeInput(TICK_Input input);
