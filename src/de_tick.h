typedef struct
{
    V2 move_dir;
    // action buttons etc will be added here
} Tick_Input;

static Tick_Input *Tick_InputAtTick(AppState *app, Uint64 tick);
