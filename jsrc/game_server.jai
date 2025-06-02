SERVER_PlayerInputs :: struct
{
  qbuf: [NET_MAX_INPUT_TICKS] TICK_PlayerInput;
  playback_range: Range(u64);
  latest_client_tick_id: u64;
  last_input: TICK_PlayerInput;
  receive_deltas: TickDeltas;
};

SERVER_State :: struct
{
  users: [NET_MAX_PLAYERS] NET_User;
  player_keys: [NET_MAX_PLAYERS] OBJ_Key;
  player_inputs: [NET_MAX_PLAYERS] SERVER_PlayerInputs;
};

SERVER_InsertPlayerInput :: (pi: *SERVER_PlayerInputs, net_msg: *NET_SendInputs, net_msg_tick_id: u64)
{
  // @refactor this is dumb and complicated!
  if (net_msg_tick_id <= pi.latest_client_tick_id)
    return; // no new inputs

  pre_insert_playback_range := Range_CountSat(pi.playback_range);

  // @threading - mutex?
  inputs := net_msg.inputs;
  input_count := min(net_msg.input_count, net_msg.inputs.count);
  skip_inputs := max(input_count - (net_msg_tick_id + 1), 0);

  first_input_tick_id := net_msg_tick_id + 1 - input_count;

  for input_index: MakeRange(skip_inputs, input_count)
  {
    input_tick := first_input_tick_id + input_index;
    if (input_tick <= pi.latest_client_tick_id)
    {
      // reject old inputs
      continue;
    }

    // store input
    pi_input := Q_Push(pi.qbuf, *pi.playback_range);
    pi_input.* = inputs[input_index];
  }

  pi.latest_client_tick_id = net_msg_tick_id;

  if (TickDeltas_AddTick(*pi.receive_deltas, net_msg_tick_id))
  {
    TickDeltas_UpdateCatchup(*pi.receive_deltas, pre_insert_playback_range.(u16));
    if (pi.receive_deltas.tick_catchup)
    {
      Nlog(LOG_NetCatchup,
          "Client (?) current input delay: % Setting input playback catchup to %",
          pre_insert_playback_range, pi.receive_deltas.tick_catchup);
    }
  }
}

SERVER_PopPlayerInput :: (pi: *SERVER_PlayerInputs) -> TICK_PlayerInput
{
  result: TICK_PlayerInput;
  pop := Q_Pop(pi.qbuf, *pi.playback_range);
  if (pop)
    result = pop.*;

  pi.last_input = result;
  return result;
}

SERVER_GetPlayerInput :: (player_index: u32) -> TICK_PlayerInput
{
  result: TICK_PlayerInput;
  if player_index >= G.server.player_inputs.count
    return result;

  pi := *G.server.player_inputs[player_index];
  playback_count := Range_CountSat(pi.playback_range);

  if playback_count > 0
    result = SERVER_PopPlayerInput(pi);
  else
  {
    Nlog(LOG_NetTick,
        "Ran out of input playback (player %u) . extrapolating; playback catchup %d",
        player_index,
        pi.receive_deltas.tick_catchup);

    // extrapolation using last input!
    result = pi.last_input;
    result.is_pathing = false;
  }

  if (playback_count > 1 &&
      pi.receive_deltas.tick_catchup > 0)
  {
    pi.receive_deltas.tick_catchup -= 1;

    next := SERVER_PopPlayerInput(pi);
    result.move_dir = result.move_dir + next.move_dir;
    if (next.is_pathing)
    {
      result.is_pathing = next.is_pathing;
      result.pathing_world_p = next.pathing_world_p;
    }
  }

  result = TICK_NormalizeInput(result);
  return result;
}
