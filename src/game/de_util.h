//
// Helpers that didn't find a better place
// live in this file for now
//

// @todo
// Turns out I'm using circle buffers a lot in this project;
// Perhaps it would be nice to create helpers for working with them;
// Needed apis (Circle_ prefix?):
// 1. Pop element
// 2. Push element
// 3. at index
// 4. is index in [min; max) range?
// 1-4 are "queue" -> create queue template?
// 5. Clear range from tick to tick;? no!

static void Circle_CopyFillRange(Uint64 elem_size,
                                 void *buf, Uint64 buf_elem_count, Uint64 *buf_elem_index,
                                 void *copy_src, Uint64 copy_src_elem_count)
{
    if (copy_src_elem_count > buf_elem_count)
    {
        // @todo support this case in the future?
        // copy just the reminder of src
        Assert(false);
        return;
    }

    Uint64 index_start = *buf_elem_index;
    Uint64 index_end = index_start + copy_src_elem_count;

    Uint64 copy1_buf_start = index_start % buf_elem_count;
    Uint64 copy1_left_in_buf = buf_elem_count - copy1_buf_start;
    Uint64 copy1_count = Min(copy1_left_in_buf, copy_src_elem_count);

    // copy 1
    {
        Uint8 *dst = (Uint8 *)buf + (copy1_buf_start * elem_size);
        memcpy(dst, copy_src, copy1_count * elem_size);
    }

    Uint64 copy2_count = copy_src_elem_count - copy1_count;
    if (copy2_count)
    {
        Uint8 *src = (Uint8 *)copy_src + (copy1_count * elem_size);
        memcpy(buf, src, copy2_count * elem_size);
    }

    *buf_elem_index = index_end;
}

//
// Queue
//
static Uint64 RngU64_Count(RngU64 rng)
{
    if (rng.max < rng.min)
        return 0;
    return rng.max - rng.min;
}

static Uint8 *Queue_Push(void *buf, Uint64 elem_count, Uint64 elem_size, RngU64 *range)
{
    Assert(range->min <= range->max);
    Uint64 current_index = range->max % elem_count;

    // truncate min if it overlaps with current
    if (range->min != range->max) // check that queue is not empty
    {
        Uint64 min_index = range->min % elem_count;
        if (min_index == current_index)
        {
            range->min += 1;
        }
    }

    // advance max
    range->max += 1;

    Uint8 *res = (Uint8 *)buf + (current_index * elem_size);
    return res;
}

static Uint8 *Queue_Peek(void *buf, Uint64 elem_count, Uint64 elem_size, RngU64 *range)
{
    Assert(range->min <= range->max);
    if (range->min >= range->max)
        return 0;

    Uint64 current_index = range->min % elem_count;
    Uint8 *res = (Uint8 *)buf + (current_index * elem_size);
    return res;
}

static Uint8 *Queue_Pop(void *buf, Uint64 elem_count, Uint64 elem_size, RngU64 *range)
{
    Uint8 *res = Queue_Peek(buf, elem_count, elem_size, range);

    // advance min
    if (res)
        range->min += 1;

    return res;
}

static Uint8 *Queue_PeekAt(void *buf, Uint64 elem_count, Uint64 elem_size, RngU64 *range, Uint64 peek_index)
{
    Assert(range->min <= range->max);
    if (range->min >= range->max)
        return 0;

    if (!(peek_index >= range->min && peek_index < range->max))
        return 0;

    Uint64 index = peek_index % elem_count;
    Uint8 *res = (Uint8 *)buf + (index * elem_size);
    return res;
}

#define Q_Push(Arr, RangePtr) (typeof(&*Arr))Queue_Push(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr)
#define Q_Pop(Arr, RangePtr) (typeof(&*Arr))Queue_Pop(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr)
#define Q_Peek(Arr, RangePtr) (typeof(&*Arr))Queue_Peek(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr)
#define Q_PeekAt(Arr, RangePtr, Index) (typeof(&*Arr))Queue_PeekAt(Arr, ArrayCount(Arr), sizeof(Arr[0]), RangePtr, Index)

//
// clamps
//
static Uint16 Saturate_U64toU16(Uint64 number)
{
    if (number > SDL_MAX_UINT16)
        return SDL_MAX_UINT16;
    return number;
}

//
// TickDeltas
//
typedef struct
{
    // This things every-increasing Uint64 ticks like tick
    // last_tick == 0 is a special tick that's assumend to
    // mean uninitialized DeltaTracker
    Uint64 last_tick;
    Uint16 deltas[64];
    Uint16 next_delta_index;
    Uint16 tick_catchup;
} TickDeltas;

static bool TickDeltas_AddTick(TickDeltas *td, Uint64 tick)
{
    if (td->last_tick == tick)
        return false;

    bool first_init = !td->last_tick;
    td->last_tick = tick;

    if (first_init)
        return false;

    Uint64 delta_u64 = (td->last_tick > tick ? 0 : tick - td->last_tick);
    Uint16 delta = Saturate_U64toU16(delta_u64);

    AssertBounds(td->next_delta_index, td->deltas);
    Uint64 index = td->next_delta_index;
    td->next_delta_index = (td->next_delta_index + 1) % ArrayCount(td->deltas);

    td->deltas[index] = delta;
    return true;
}

static void TickDeltas_UpdateCatchup(TickDeltas *td, Uint16 current_delta)
{
    if (!td->last_tick)
        return;

    Uint16 max_delta = 0;
    ForArray(i, td->deltas)
    {
        if (td->deltas[i] > max_delta)
            max_delta = td->deltas[i];
    }

    Uint16 target_delta = max_delta + (max_delta / 16) + 4;
    if (current_delta > target_delta)
    {
        td->tick_catchup = current_delta - target_delta;
    }
    else
    {
        td->tick_catchup = 0;
    }
}
