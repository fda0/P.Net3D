static void Server_InsertPlayerInput(Server_PlayerInputs *pi, Net_SendInputs *net_msg, Uint64 net_msg_tick_id)
{
    if (net_msg_tick_id <= pi->latest_client_tick_id)
        return; // no new inputs

    Uint64 pre_insert_playback_range = RngU64_Count(pi->playback_range);

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
        if (input_tick <= pi->latest_client_tick_id)
        {
            // reject old inputs
            continue;
        }

        stored_inputs += 1;

        // store input
        Tick_Input *pi_input = Q_Push(pi->qbuf, &pi->playback_range);
        *pi_input = inputs[input_index];
    }

#if 0
    LOG(LogFlags_NetCatchup,
        "%s: Client (?) stored inputs: %llu, "
        "received net_msg_tick: %llu, latest saved tick: %llu",
        Net_Label2(),
        stored_inputs,
        net_msg_tick_id,
        pi->latest_client_tick_id);
#endif

    pi->latest_client_tick_id = net_msg_tick_id;

    if (TickDeltas_AddTick(&pi->receive_deltas, net_msg_tick_id))
    {
        TickDeltas_UpdateCatchup(&pi->receive_deltas, pre_insert_playback_range);
        if (pi->receive_deltas.tick_catchup)
        {
            LOG(LogFlags_NetCatchup,
                "%s: Client (?) current input delay: %llu"
                " Setting input playback catchup to %d",
                Net_Label2(),
                pre_insert_playback_range,
                (int)pi->receive_deltas.tick_catchup);
        }
    }
}

static Tick_Input Server_PopPlayerInput(Server_PlayerInputs *pi)
{
    Tick_Input result = {};
    Tick_Input *pop = Q_Pop(pi->qbuf, &pi->playback_range);
    if (pop)
        result = *pop;

    pi->last_input = result;
    return result;
}

static Tick_Input Server_GetPlayerInput(AppState *app, Uint32 player_index)
{
    Tick_Input result = {};
    if (player_index >= ArrayCount(app->server.player_inputs))
        return result;

    Server_PlayerInputs *pi = app->server.player_inputs + player_index;
    Uint64 playback_count = RngU64_Count(pi->playback_range);

    if (playback_count > 0)
    {
        result = Server_PopPlayerInput(pi);
    }
    else
    {
        LOG(LogFlags_NetTick,
            "%s: Ran out of input playback -> extrapolating; "
            "playback catchup %d",
            Net_Label2(),
            pi->receive_deltas.tick_catchup);

        // extrapolation using last input!
        result = pi->last_input;
        result.is_pathing = false;
    }

#if 1
    if (playback_count > 1 &&
        pi->receive_deltas.tick_catchup > 0)
    {
        pi->receive_deltas.tick_catchup -= 1;

        Tick_Input next = Server_PopPlayerInput(pi);
        result.move_dir = V2_Add(result.move_dir, next.move_dir);
        if (next.is_pathing)
        {
            result.is_pathing = next.is_pathing;
            result.pathing_world_p = next.pathing_world_p;
        }
    }
#endif

    result = Tick_NormalizeInput(result);
    return result;
}
