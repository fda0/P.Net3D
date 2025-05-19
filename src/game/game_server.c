static void SERVER_InsertPlayerInput(SERVER_PlayerInputs *pi, NET_SendInputs *net_msg, U64 net_msg_tick_id)
{
  if (net_msg_tick_id <= pi->latest_client_tick_id)
    return; // no new inputs

  U64 pre_insert_playback_range = RngU64_Count(pi->playback_range);

  // @threading - mutex?
  TICK_Input *inputs = net_msg->inputs;
  U16 input_count = Min(net_msg->input_count, ArrayCount(net_msg->inputs));

  if (net_msg_tick_id + 1 < input_count)
  {
    U16 new_input_count = Checked_U64toU16(net_msg_tick_id + 1);
    U16 delta = input_count - new_input_count;

    input_count = new_input_count;
    inputs += delta;
  }

  U64 first_input_tick_id = net_msg_tick_id + 1 - input_count;

  ForU64(input_index, input_count)
  {
    U64 input_tick = first_input_tick_id + input_index;
    if (input_tick <= pi->latest_client_tick_id)
    {
      // reject old inputs
      continue;
    }

    // store input
    TICK_Input *pi_input = (TICK_Input *)Q_Push(pi->qbuf, &pi->playback_range);
    *pi_input = inputs[input_index];
  }

  pi->latest_client_tick_id = net_msg_tick_id;

  if (TickDeltas_AddTick(&pi->receive_deltas, net_msg_tick_id))
  {
    TickDeltas_UpdateCatchup(&pi->receive_deltas, Checked_U64toU16(pre_insert_playback_range));
    if (pi->receive_deltas.tick_catchup)
    {
      LOG(LOG_NetCatchup,
          "%s: Client (?) current input delay: %llu"
          " Setting input playback catchup to %d",
          NET_Label(),
          pre_insert_playback_range,
          (int)pi->receive_deltas.tick_catchup);
    }
  }
}

static TICK_Input SERVER_PopPlayerInput(SERVER_PlayerInputs *pi)
{
  TICK_Input result = {};
  TICK_Input *pop = (TICK_Input *)Q_Pop(pi->qbuf, &pi->playback_range);
  if (pop)
    result = *pop;

  pi->last_input = result;
  return result;
}

static TICK_Input SERVER_GetPlayerInput(U32 player_index)
{
  TICK_Input result = {};
  if (player_index >= ArrayCount(APP.server.player_inputs))
    return result;

  SERVER_PlayerInputs *pi = APP.server.player_inputs + player_index;
  U64 playback_count = RngU64_Count(pi->playback_range);

  if (playback_count > 0)
  {
    result = SERVER_PopPlayerInput(pi);
  }
  else
  {
    LOG(LOG_NetTick,
        "%s: Ran out of input playback (player %u) -> extrapolating; "
        "playback catchup %d",
        NET_Label(),
        player_index,
        pi->receive_deltas.tick_catchup);

    // extrapolation using last input!
    result = pi->last_input;
    result.is_pathing = false;
  }

  if (playback_count > 1 &&
      pi->receive_deltas.tick_catchup > 0)
  {
    pi->receive_deltas.tick_catchup -= 1;

    TICK_Input next = SERVER_PopPlayerInput(pi);
    result.move_dir = V2_Add(result.move_dir, next.move_dir);
    if (next.is_pathing)
    {
      result.is_pathing = next.is_pathing;
      result.pathing_world_p = next.pathing_world_p;
    }
  }

  result = TICK_NormalizeInput(result);
  return result;
}
