SERVER_PlayerInputs :: struct
{
  input_queue: Queue(NET_MAX_INPUT_TICKS, TICK_PlayerInput);
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
  if net_msg_tick_id <= pi.latest_client_tick_id
    return; // no new inputs

  pre_insert_input_queue_count := pi.input_queue.count;

  // @threading - mutex?
  inputs := net_msg.inputs;
  input_count := min(net_msg.input_count, net_msg.inputs.count);
  skip_inputs: u64;
  if input_count > net_msg_tick_id + 1
    skip_inputs = input_count - (net_msg_tick_id + 1);

  first_input_tick_id := net_msg_tick_id + 1 - input_count;

  for input_index: MakeRange(skip_inputs, input_count)
  {
    input_tick := first_input_tick_id + input_index;
    if input_tick <= pi.latest_client_tick_id
    {
      // reject old inputs
      continue;
    }

    // store input
    QueuePush(*pi.input_queue, inputs[input_index]);
  }

  pi.latest_client_tick_id = net_msg_tick_id;

  if TickDeltas_AddTick(*pi.receive_deltas, net_msg_tick_id)
  {
    TickDeltas_UpdateCatchup(*pi.receive_deltas, xx pre_insert_input_queue_count);
    if pi.receive_deltas.tick_catchup
    {
      Nlog(LOG_NetCatchup,
          "Client (?) current input delay: % Setting input playback catchup to %",
          pre_insert_input_queue_count, pi.receive_deltas.tick_catchup);
    }
  }
}

SERVER_PopPlayerInput :: (pi: *SERVER_PlayerInputs) -> TICK_PlayerInput
{
  popped := QueuePop(*pi.input_queue);
  pi.last_input = popped;
  return popped;
}

SERVER_GetPlayerInput :: (player_index: u32) -> TICK_PlayerInput
{
  result: TICK_PlayerInput;
  if player_index >= G.server.player_inputs.count
    return result;

  pi := *G.server.player_inputs[player_index];
  starting_input_count := pi.input_queue.count;

  if starting_input_count > 0
    result = SERVER_PopPlayerInput(pi);
  else
  {
    Nlog(LOG_NetTick,
        "Ran out of input playback (player %) -> extrapolating; playback catchup %",
        player_index, pi.receive_deltas.tick_catchup);

    // extrapolation using last input!
    result = pi.last_input;
    result.is_pathing = false;
  }

  if starting_input_count > 1 && pi.receive_deltas.tick_catchup > 0
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
