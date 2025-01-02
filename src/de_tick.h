typedef struct
{
    V2 move_dir;
    // action buttons etc will be added here
    bool is_pathing;
    V2 pathing_world_p;
} Tick_Input;

static Tick_Input Tick_NormalizeInput(Tick_Input input);
