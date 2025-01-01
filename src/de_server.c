static void Server_PlayerInputBufferInsert(Server_PlayerInputBuffer *in_buf,
                                           Net_SendInputs *net_msg, Uint64 net_msg_tick_id)
{
    // @threading - mutex?

    Uint16 input_count = net_msg->input_count;

    if (input_count > ArrayCount(net_msg->inputs))
    {
        Assert(false);
        input_count = ArrayCount(net_msg->inputs);
    }
    if (net_msg_tick_id + 1 < input_count)
    {
        Assert(false);
        input_count = net_msg_tick_id + 1;
    }

    if (net_msg_tick_id <= in_buf->latest_client_tick_id)
        return; // no new inputs

    Uint64 inputs_start_tick_id = net_msg_tick_id + 1 - input_count;

    ForU64(input_index, input_count)
    {
        Uint64 input_tick = inputs_start_tick_id + input_index;
        if (input_tick < in_buf->latest_client_tick_id)
        {
            // reject old inputs
            continue;
        }

        // select slot from circular buffer
        Uint64 current = in_buf->playback_index_max % ArrayCount(in_buf->circle_inputs);

        // advance min
        {
            Uint64 min = in_buf->playback_index_min % ArrayCount(in_buf->circle_inputs);
            if (in_buf->playback_index_min != in_buf->playback_index_max &&
                min == current)
            {
                in_buf->playback_index_min += 1;
            }
        }
        // advance max
        in_buf->playback_index_max += 1;

        // store input value
        {
            Tick_Input net_input = net_msg->inputs[input_index];
            net_input = Tick_NormalizeInput(net_input);
            in_buf->circle_inputs[current] = net_input;
        }
    }

    in_buf->latest_client_tick_id = net_msg_tick_id;
}

static Uint64 Server_PlayerInputBuffer_ItemCount(Server_PlayerInputBuffer *input)
{
    Assert(input->playback_index_min <= input->playback_index_max);
    return input->playback_index_max - input->playback_index_min;
}

static Tick_Input Server_PlayerInputBuffer_Pop(Server_PlayerInputBuffer *input)
{
    Tick_Input result = {};
    if (input->playback_index_min < input->playback_index_max)
    {
        Uint64 min = input->playback_index_min % ArrayCount(input->circle_inputs);
        result = input->circle_inputs[min];
        input->playback_index_min += 1;
    }
    return result;
}

static Tick_Input Server_PlayerInputBuffer_Extrapolate(Server_PlayerInputBuffer *input)
{
    Tick_Input result = {};
    if (input->playback_index_min == 0)
        return result;

    Uint64 playback_index_prev = input->playback_index_min - 1;
    Uint64 index = playback_index_prev % ArrayCount(input->circle_inputs);

    result = input->circle_inputs[index];
    // @todo clear one-off actions here
    return result;
}

static Tick_Input Server_GetPlayerInput(AppState *app, Uint32 player_index)
{
    Tick_Input result = {};
    if (player_index >= ArrayCount(app->server.player_inputs))
        return result;

    Server_PlayerInputBuffer *input = app->server.player_inputs + player_index;
    Uint64 count = Server_PlayerInputBuffer_ItemCount(input);

    if (count > 2) // @todo this value should be dynamic and aim to be stable, simillary to how client handles playback
    {
        Tick_Input a = Server_PlayerInputBuffer_Pop(input);
        Tick_Input b = Server_PlayerInputBuffer_Pop(input);
        result.move_dir = V2_Add(a.move_dir, b.move_dir);
        result = Tick_NormalizeInput(result);
    }
    else if (count > 0)
    {
        result = Server_PlayerInputBuffer_Pop(input);
    }
    else
    {
        result = Server_PlayerInputBuffer_Extrapolate(input);
    }

    return result;
}
