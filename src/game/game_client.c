static Obj_Sync *Client_SnapshotObjSyncAtTick(Client_Snapshot *snap, U64 tick_id)
{
    U64 state_index = tick_id % ArrayCount(snap->tick_states);
    return snap->tick_states + state_index;
}

static Obj_Sync Client_LerpNetObjSync(U32 net_index, U64 tick_id)
{
    Assert(net_index < ArrayCount(APP.client.obj_snaps));
    Client_Snapshot *snap = APP.client.obj_snaps + net_index;

    Assert(tick_id <= snap->latest_server_tick &&
           tick_id >= snap->oldest_server_tick);

    Obj_Sync *exact_obj = Client_SnapshotObjSyncAtTick(snap, tick_id);
    if (Obj_SyncIsInit(exact_obj)) // exact object found
        return *exact_obj;

    // find nearest prev_id/next_id objects
    U64 prev_id = tick_id;
    U64 next_id = tick_id;
    while (prev_id > snap->oldest_server_tick)
    {
        prev_id -= 1;
        if (Obj_SyncIsInit(Client_SnapshotObjSyncAtTick(snap, prev_id)))
            break;
    }
    while (next_id < snap->latest_server_tick)
    {
        next_id += 1;
        if (Obj_SyncIsInit(Client_SnapshotObjSyncAtTick(snap, next_id)))
            break;
    }

    Obj_Sync *prev = Client_SnapshotObjSyncAtTick(snap, prev_id);
    Obj_Sync *next = Client_SnapshotObjSyncAtTick(snap, next_id);

    // handle cases where we can't interpolate
    if (!Obj_SyncIsInit(prev) && !Obj_SyncIsInit(next)) // disaster
        return *exact_obj;

    if (!Obj_SyncIsInit(prev))
        return *next;

    if (!Obj_SyncIsInit(next))
        return *prev;

    // lock lerp range; new packets in this range will be rejected
    snap->recent_lerp_start_tick = prev_id;
    snap->recent_lerp_end_tick = next_id;

    // do the interpolation
    U64 id_range = next_id - prev_id;
    U64 id_offset = tick_id - prev_id;
    float t = (float)id_offset / (float)id_range;
    Obj_Sync result = Obj_SyncLerp(*prev, *next, t);
    return result;
}

static bool Client_InsertSnapshotObjSync(Client_Snapshot *snap, U64 insert_at_tick_id, Obj_Sync insert_sync)
{
    // function returns true on error
    static_assert(ArrayCount(snap->tick_states) == NET_CLIENT_MAX_SNAPSHOTS);
    U64 last_to_first_tick_offset = NET_CLIENT_MAX_SNAPSHOTS - 1;

    if (snap->recent_lerp_start_tick != snap->recent_lerp_end_tick &&
        insert_at_tick_id >= snap->recent_lerp_start_tick &&
        insert_at_tick_id <= snap->recent_lerp_end_tick)
    {
        LOG(LogFlags_NetClient,
            "%s: Rejecting snapshot insert (in the middle, locked by lerp) - "
            "latest server tick: %llu; "
            "insert tick: %llu; "
            "diff: %llu (max: %d)",
            Net_Label(), snap->latest_server_tick, insert_at_tick_id,
            snap->latest_server_tick - insert_at_tick_id, (int)NET_CLIENT_MAX_SNAPSHOTS);
        return true;
    }

    I64 delta_from_latest = (I64)insert_at_tick_id - (I64)snap->latest_server_tick;
    if (delta_from_latest > 0)
    {
        // save new latest server tick
        snap->latest_server_tick = insert_at_tick_id;
    }

    U64 minimum_server_tick = (snap->latest_server_tick >= last_to_first_tick_offset ?
                               snap->latest_server_tick - last_to_first_tick_offset :
                               0);

    if (insert_at_tick_id < minimum_server_tick)
    {
        Assert(delta_from_latest <= 0);
        LOG(LogFlags_NetClient,
            "%s: Rejecting snapshot insert (underflow) - "
            "latest server tick: %llu; "
            "insert tick: %llu; "
            "diff: %llu (max: %llu)",
            Net_Label(), snap->latest_server_tick, insert_at_tick_id,
            insert_at_tick_id - snap->latest_server_tick,
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
            SDL_zeroa(snap->tick_states);
        }
        else
        {
            U64 clearing_start = insert_at_tick_id - delta_from_latest + 1;
            for (U64 i = clearing_start;
                 i < insert_at_tick_id;
                 i += 1)
            {
                Obj_Sync *obj = Client_SnapshotObjSyncAtTick(snap, i);
                SDL_zerop(obj);
            }
        }


        // recalculate oldest_server_tick
        if (snap->oldest_server_tick < minimum_server_tick)
        {
            // find new oldest
            for (U64 i = minimum_server_tick;
                 i < insert_at_tick_id;
                 i += 1)
            {
                Obj_Sync *obj = Client_SnapshotObjSyncAtTick(snap, i);
                if (Obj_SyncIsInit(obj))
                {
                    snap->oldest_server_tick = i;
                    break;
                }
            }

            // failed to find oldest
            if (snap->oldest_server_tick < minimum_server_tick)
            {
                snap->oldest_server_tick = insert_at_tick_id;
            }
        }
    }

    // update oldest if inserted obj is older than oldest
    if (snap->oldest_server_tick > insert_at_tick_id)
    {
        snap->oldest_server_tick = insert_at_tick_id;
    }

    // insert object
    {
        Obj_Sync *insert_at_obj = Client_SnapshotObjSyncAtTick(snap, insert_at_tick_id);
        *insert_at_obj = insert_sync;
    }

    Assert(snap->oldest_server_tick >= minimum_server_tick);
    return false; // no error
}

static Tick_Input *Client_PollInput()
{
    V2 dir = {0};
    if (APP.keyboard[SDL_SCANCODE_W] || APP.keyboard[SDL_SCANCODE_UP])    dir.x += 1;
    if (APP.keyboard[SDL_SCANCODE_S] || APP.keyboard[SDL_SCANCODE_DOWN])  dir.x -= 1;
    if (APP.keyboard[SDL_SCANCODE_A] || APP.keyboard[SDL_SCANCODE_LEFT])  dir.y += 1;
    if (APP.keyboard[SDL_SCANCODE_D] || APP.keyboard[SDL_SCANCODE_RIGHT]) dir.y -= 1;

    Tick_Input *input = Q_Push(APP.client.inputs_qbuf, &APP.client.inputs_range);
    SDL_zerop(input);

    input->move_dir = V2_Normalize(dir);
    if (APP.pathing_marker_set)
    {
        APP.pathing_marker_set = false;
        input->is_pathing = true;
        input->pathing_world_p = Obj_Get(APP.pathing_marker, ObjStorage_Local)->s.p;
    }

    return input;
}
