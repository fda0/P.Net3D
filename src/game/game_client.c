static OBJ_Sync *CLIENT_ObjSyncAtTick(CLIENT_ObjSnapshots *snaps, U64 tick_id)
{
  U64 state_index = tick_id % ArrayCount(snaps->tick_states);
  return snaps->tick_states + state_index;
}

static OBJ_Sync CLIENT_LerpObjSync(U32 net_index, U64 tick_id)
{
  Assert(net_index < ArrayCount(APP.client.snaps_of_objs));
  CLIENT_ObjSnapshots *snaps = APP.client.snaps_of_objs + net_index;

  Assert(tick_id <= snaps->latest_server_tick &&
         tick_id >= snaps->oldest_server_tick);

  OBJ_Sync *exact_obj = CLIENT_ObjSyncAtTick(snaps, tick_id);
  if (OBJ_SyncIsInit(exact_obj)) // exact object found
    return *exact_obj;

  // find nearest prev_id/next_id objects
  U64 prev_id = tick_id;
  U64 next_id = tick_id;
  while (prev_id > snaps->oldest_server_tick)
  {
    prev_id -= 1;
    if (OBJ_SyncIsInit(CLIENT_ObjSyncAtTick(snaps, prev_id)))
      break;
  }
  while (next_id < snaps->latest_server_tick)
  {
    next_id += 1;
    if (OBJ_SyncIsInit(CLIENT_ObjSyncAtTick(snaps, next_id)))
      break;
  }

  OBJ_Sync *prev = CLIENT_ObjSyncAtTick(snaps, prev_id);
  OBJ_Sync *next = CLIENT_ObjSyncAtTick(snaps, next_id);

  // handle cases where we can't interpolate
  if (!OBJ_SyncIsInit(prev) && !OBJ_SyncIsInit(next)) // disaster
    return *exact_obj;

  if (!OBJ_SyncIsInit(prev))
    return *next;

  if (!OBJ_SyncIsInit(next))
    return *prev;

  // lock lerp range; new packets in this range will be rejected
  snaps->recent_lerp_start_tick = prev_id;
  snaps->recent_lerp_end_tick = next_id;

  // do the interpolation
  U64 id_range = next_id - prev_id;
  U64 id_offset = tick_id - prev_id;
  float t = (float)id_offset / (float)id_range;
  OBJ_Sync result = OBJ_SyncLerp(*prev, *next, t);
  return result;
}

static bool CLIENT_InsertSnapshot(CLIENT_ObjSnapshots *snaps, U64 insert_at_tick_id, OBJ_Sync new_value)
{
  // function returns true on error
  static_assert(ArrayCount(snaps->tick_states) == NET_CLIENT_MAX_SNAPSHOTS);
  U64 last_to_first_tick_offset = NET_CLIENT_MAX_SNAPSHOTS - 1;

  if (snaps->recent_lerp_start_tick != snaps->recent_lerp_end_tick &&
      insert_at_tick_id >= snaps->recent_lerp_start_tick &&
      insert_at_tick_id <= snaps->recent_lerp_end_tick)
  {
    LOG(Log_NetClient,
        "%s: Rejecting snapshot insert (in the middle, locked by lerp) - "
        "latest server tick: %llu; "
        "insert tick: %llu; "
        "diff: %llu (max: %d)",
        NET_Label(), snaps->latest_server_tick, insert_at_tick_id,
        snaps->latest_server_tick - insert_at_tick_id, (int)NET_CLIENT_MAX_SNAPSHOTS);
    return true;
  }

  I64 delta_from_latest = (I64)insert_at_tick_id - (I64)snaps->latest_server_tick;
  if (delta_from_latest > 0)
  {
    // save new latest server tick
    snaps->latest_server_tick = insert_at_tick_id;
  }

  U64 minimum_server_tick = (snaps->latest_server_tick >= last_to_first_tick_offset ?
                             snaps->latest_server_tick - last_to_first_tick_offset :
                             0);

  if (insert_at_tick_id < minimum_server_tick)
  {
    Assert(delta_from_latest <= 0);
    LOG(Log_NetClient,
        "%s: Rejecting snapshot insert (underflow) - "
        "latest server tick: %llu; "
        "insert tick: %llu; "
        "diff: %llu (max: %llu)",
        NET_Label(), snaps->latest_server_tick, insert_at_tick_id,
        insert_at_tick_id - snaps->latest_server_tick,
        (U64)NET_CLIENT_MAX_SNAPSHOTS);
    return true;
  }

  if (delta_from_latest > 0)
  {
    // tick_id is newer than latest_server_at_tick
    // zero-out the gap between newly inserted object and previous latest server tick

    if (delta_from_latest >= NET_CLIENT_MAX_SNAPSHOTS)
    {
      // optimized branch
      // tick_id is much-newer than latest_server_at_tick
      // we can safely clear the whole circle buffer
      SDL_zeroa(snaps->tick_states);
    }
    else
    {
      U64 clearing_start = insert_at_tick_id - delta_from_latest + 1;
      for (U64 i = clearing_start;
           i < insert_at_tick_id;
           i += 1)
      {
        OBJ_Sync *obj = CLIENT_ObjSyncAtTick(snaps, i);
        SDL_zerop(obj);
      }
    }


    // recalculate oldest_server_tick
    if (snaps->oldest_server_tick < minimum_server_tick)
    {
      // find new oldest
      for (U64 i = minimum_server_tick;
           i < insert_at_tick_id;
           i += 1)
      {
        OBJ_Sync *obj = CLIENT_ObjSyncAtTick(snaps, i);
        if (OBJ_SyncIsInit(obj))
        {
          snaps->oldest_server_tick = i;
          break;
        }
      }

      // failed to find oldest
      if (snaps->oldest_server_tick < minimum_server_tick)
      {
        snaps->oldest_server_tick = insert_at_tick_id;
      }
    }
  }

  // update oldest if inserted obj is older than oldest
  if (snaps->oldest_server_tick > insert_at_tick_id)
  {
    snaps->oldest_server_tick = insert_at_tick_id;
  }

  // insert object
  {
    OBJ_Sync *insert_at_obj = CLIENT_ObjSyncAtTick(snaps, insert_at_tick_id);
    *insert_at_obj = new_value;
  }

  Assert(snaps->oldest_server_tick >= minimum_server_tick);
  return false; // no error
}

static TICK_Input *CLIENT_PollInput()
{
  // @todo input polling from SDL/OS should be done here directly?
  // Will make sense to revisit it when implementing multithreading.
  V2 dir = {0};
  if (KEY_Held(SDL_SCANCODE_W)) dir.x += 1;
  if (KEY_Held(SDL_SCANCODE_S)) dir.x -= 1;
  if (KEY_Held(SDL_SCANCODE_A)) dir.y += 1;
  if (KEY_Held(SDL_SCANCODE_D)) dir.y -= 1;

  TICK_Input *input = (TICK_Input *)Q_Push(APP.client.inputs_qbuf, &APP.client.inputs_range);
  SDL_zerop(input);

  input->move_dir = V2_Normalize(dir);
  if (APP.pathing_marker_set)
  {
    APP.pathing_marker_set = false;
    input->is_pathing = true;
    input->pathing_world_p = V2_FromV3_XY(OBJ_Get(APP.pathing_marker, ObjStorage_Local)->s.p);
  }

  return input;
}
