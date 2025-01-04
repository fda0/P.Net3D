static void Server_PlayerInputBufferInsert(Server_PlayerInputBuffer *inbuf,
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

    if (net_msg_tick_id <= inbuf->latest_client_tick_id)
        return; // no new inputs

    Uint64 inputs_start_tick_id = net_msg_tick_id + 1 - input_count;

    ForU64(input_index, input_count)
    {
        Uint64 input_tick = inputs_start_tick_id + input_index;
        if (input_tick < inbuf->latest_client_tick_id)
        {
            // reject old inputs
            continue;
        }

        // store input value
        Tick_Input net_input = net_msg->inputs[input_index];
        net_input = Tick_NormalizeInput(net_input);

        Tick_Input *inbuf_input = Q_Push(inbuf->qbuf, &inbuf->playback_range);
        *inbuf_input = net_input;
    }

    inbuf->latest_client_tick_id = net_msg_tick_id;
}

static Tick_Input Server_PlayerInputBuffer_Pop(Server_PlayerInputBuffer *input)
{
    Tick_Input result = {};
    Tick_Input *pop = Q_Pop(input->qbuf, &input->playback_range);
    if (pop)
        result = *pop;

    input->last_input = result;
    return result;
}

static Tick_Input Server_GetPlayerInput(AppState *app, Uint32 player_index)
{
    Tick_Input result = {};
    if (player_index >= ArrayCount(app->server.player_inputs))
        return result;

    Server_PlayerInputBuffer *inbuf = app->server.player_inputs + player_index;
    Uint64 playback_count = RngU64_Count(inbuf->playback_range);

    if (playback_count > 0)
    {
        result = Server_PlayerInputBuffer_Pop(inbuf);
    }
    else
    {
        result = inbuf->last_input;
        // extrapolation using last input!
        // @todo must require special handling;
        // inputs should be filtered here for sure!
        // having input extrapolation makes sense if the player moves
        // using WASD etc; but for other kinds of inputs it isn't desired
    }

    // @todo this value should be dynamic and aim to be stable, simillary to how client handles playback
    // speed up playback; merge 2 inputs
    if (playback_count > 2)
    {
        Tick_Input next = Server_PlayerInputBuffer_Pop(inbuf);
        result.move_dir = V2_Add(result.move_dir, next.move_dir);
        result = Tick_NormalizeInput(result);
    }

    return result;
}
