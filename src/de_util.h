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
