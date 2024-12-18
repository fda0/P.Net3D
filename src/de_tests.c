static void Test_Iterate()
{
    // CircleBufferFill
    {
        Uint16 memory[4 * 3] = {0}; // 4B padding, 4B buf, 4B padding
        Uint16 *circle_buf = memory + 4;
        Uint64 index = 0;

        // test 1
        {
            Uint16 copy[] = {1,2,3,4};
            CircleBufferFill(sizeof(Uint16), circle_buf, 4, &index, copy, ArrayCount(copy));
        }
        {
            Uint16 ref[] = {0,0,0,0, 1,2,3,4, 0,0,0,0};
            Assert(index == 4);
            Assert(memcmp(memory, ref, sizeof(ref)) == 0);
        }

        // test 2
        {
            Uint16 copy[] = {10,11,12};
            CircleBufferFill(sizeof(Uint16), circle_buf, 4, &index, copy, ArrayCount(copy));
        }
        {
            Uint16 ref[] = {0,0,0,0, 10,11,12,4, 0,0,0,0};
            Assert(index == 7);
            Assert(memcmp(memory, ref, sizeof(ref)) == 0);
        }

        // test 3
        {
            Uint16 copy[] = {20,21,22,23};
            CircleBufferFill(sizeof(Uint16), circle_buf, 4, &index, copy, ArrayCount(copy));
        }
        {
            Uint16 ref[] = {0,0,0,0, 21,22,23,20, 0,0,0,0};
            Assert(index == 11);
            Assert(memcmp(memory, ref, sizeof(ref)) == 0);
        }
    }
}
