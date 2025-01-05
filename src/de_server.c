static void Server_PlayerInputBufferInsert(Server_PlayerInputBuffer *inbuf,
                                           Net_SendInputs *net_msg, Uint64 net_msg_tick_id)
{
    if (net_msg_tick_id <= inbuf->latest_client_tick_id)
        return; // no new inputs

    Uint64 pre_insert_playback_range = RngU64_Count(inbuf->playback_range);

    // @threading - mutex?
    Tick_Input *inputs = net_msg->inputs;
    Uint16 input_count = Min(net_msg->input_count, ArrayCount(net_msg->inputs));

    if (net_msg_tick_id + 1 < input_count)
    {
        Uint16 new_input_count = net_msg_tick_id + 1;
        Uint16 delta = input_count - new_input_count;

        input_count = new_input_count;
        inputs += delta;
    }

    Uint64 first_input_tick_id = net_msg_tick_id + 1 - input_count;
    Uint64 stored_inputs = 0;

    ForU64(input_index, input_count)
    {
        Uint64 input_tick = first_input_tick_id + input_index;
        if (input_tick <= inbuf->latest_client_tick_id)
        {
            // reject old inputs
            continue;
        }

        stored_inputs += 1;

        // prepare input
        Tick_Input net_input = Tick_NormalizeInput(inputs[input_index]);

        // store input
        Tick_Input *inbuf_input = Q_Push(inbuf->qbuf, &inbuf->playback_range);
        *inbuf_input = net_input;
    }

#if 0
    LOG(LogFlags_NetCatchup,
        "%s: Client (?) stored inputs: %llu, "
        "received net_msg_tick: %llu, latest saved tick: %llu",
        Net_Label2(),
        stored_inputs,
        net_msg_tick_id,
        inbuf->latest_client_tick_id);
#endif

    inbuf->latest_client_tick_id = net_msg_tick_id;

    if (TickDeltas_AddTick(&inbuf->receive_deltas, net_msg_tick_id))
    {
        TickDeltas_UpdateCatchup(&inbuf->receive_deltas, pre_insert_playback_range);
        if (inbuf->receive_deltas.tick_catchup)
        {
            LOG(LogFlags_NetCatchup,
                "%s: Client (?) current input delay: %llu"
                " Setting input playback catchup to %d",
                Net_Label2(),
                pre_insert_playback_range,
                (int)inbuf->receive_deltas.tick_catchup);
        }
    }

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
        LOG(LogFlags_NetTick,
            "%s: Ran out of input playback - now extrapolating; "
            "playback catchup %d",
            Net_Label2(),
            inbuf->receive_deltas.tick_catchup);

        result = inbuf->last_input;

        // extrapolation using last input!
        // @todo must require special handling;
        // inputs should be filtered here for sure!
        // having input extrapolation makes sense if the player moves
        // using WASD etc; but for other kinds of inputs it isn't desired
    }

#if 1
    if (playback_count > 1 &&
        inbuf->receive_deltas.tick_catchup > 0)
    {
        inbuf->receive_deltas.tick_catchup -= 1;

        Tick_Input next = Server_PlayerInputBuffer_Pop(inbuf);
        result.move_dir = V2_Add(result.move_dir, next.move_dir);
        result = Tick_NormalizeInput(result);
    }
#endif

    return result;
}
