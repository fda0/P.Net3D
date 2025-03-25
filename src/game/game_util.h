typedef struct
{
  // This tracks things like every-increasing U64 ticks
  // last_tick == 0 is a special case that will trigger
  // init code
  U64 last_tick;
  U16 deltas[64];
  U16 next_delta_index;
  U16 tick_catchup;
} TickDeltas;
