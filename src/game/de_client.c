static Object *Client_SnapshotObjectAtTick(Client_Snapshot *snap, Uint64 tick_id)
{
    Uint64 state_index = tick_id % ArrayCount(snap->tick_states);
    return snap->tick_states + state_index;
}

static Object Client_LerpNetObject(AppState *app, Uint32 net_index, Uint64 tick_id)
{
    Assert(net_index < ArrayCount(app->client.obj_snaps));
    Client_Snapshot *snap = app->client.obj_snaps + net_index;

    Assert(tick_id <= snap->latest_server_tick &&
           tick_id >= snap->oldest_server_tick);

    Object *exact_obj = Client_SnapshotObjectAtTick(snap, tick_id);
    if (Object_IsInit(exact_obj)) // exact object found
        return *exact_obj;

    // find nearest prev_id/next_id objects
    Uint64 prev_id = tick_id;
    Uint64 next_id = tick_id;
    while (prev_id > snap->oldest_server_tick)
    {
        prev_id -= 1;
        if (Object_IsInit(Client_SnapshotObjectAtTick(snap, prev_id)))
            break;
    }
    while (next_id < snap->latest_server_tick)
    {
        next_id += 1;
        if (Object_IsInit(Client_SnapshotObjectAtTick(snap, next_id)))
            break;
    }

    Object *prev = Client_SnapshotObjectAtTick(snap, prev_id);
    Object *next = Client_SnapshotObjectAtTick(snap, next_id);

    // handle cases where we can't interpolate
    if (!Object_IsInit(prev) && !Object_IsInit(next)) // disaster
        return *exact_obj;

    if (!Object_IsInit(prev))
        return *next;

    if (!Object_IsInit(next))
        return *prev;

    // lock lerp range; new packets in this range will be rejected
    snap->recent_lerp_start_tick = prev_id;
    snap->recent_lerp_end_tick = next_id;

    // do the interpolation
    Uint64 id_range = next_id - prev_id;
    Uint64 id_offset = tick_id - prev_id;
    float t = (float)id_offset / (float)id_range;
    Object result = Object_Lerp(*prev, *next, t);
    return result;
}

static bool Client_InsertSnapshotObject(AppState *app, Client_Snapshot *snap,
                                        Uint64 insert_at_tick_id, Object insert_obj)
{
    // function returns true on error
    static_assert(ArrayCount(snap->tick_states) == NET_CLIENT_MAX_SNAPSHOTS);
    Uint64 last_to_first_tick_offset = NET_CLIENT_MAX_SNAPSHOTS - 1;

    if (snap->recent_lerp_start_tick != snap->recent_lerp_end_tick &&
        insert_at_tick_id >= snap->recent_lerp_start_tick &&
        insert_at_tick_id <= snap->recent_lerp_end_tick)
    {
        LOG(LogFlags_NetClient,
            "%s: Rejecting snapshot insert (in the middle, locked by lerp) - "
            "latest server tick: %llu; "
            "insert tick: %llu; "
            "diff: %llu (max: %d)",
            Net_Label(app), snap->latest_server_tick, insert_at_tick_id,
            snap->latest_server_tick - insert_at_tick_id, (int)NET_CLIENT_MAX_SNAPSHOTS);
        return true;
    }

    Sint64 delta_from_latest = (Sint64)insert_at_tick_id - (Sint64)snap->latest_server_tick;
    if (delta_from_latest > 0)
    {
        // save new latest server tick
        snap->latest_server_tick = insert_at_tick_id;
    }

    Uint64 minimum_server_tick = (snap->latest_server_tick >= last_to_first_tick_offset ?
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
            Net_Label(app), snap->latest_server_tick, insert_at_tick_id,
            insert_at_tick_id - snap->latest_server_tick,
            (Uint64)NET_CLIENT_MAX_SNAPSHOTS);
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
            Uint64 clearing_start = insert_at_tick_id - delta_from_latest + 1;
            for (Uint64 i = clearing_start;
                 i < insert_at_tick_id;
                 i += 1)
            {
                Object *obj = Client_SnapshotObjectAtTick(snap, i);
                SDL_zerop(obj);
            }
        }


        // recalculate oldest_server_tick
        if (snap->oldest_server_tick < minimum_server_tick)
        {
            // find new oldest
            for (Uint64 i = minimum_server_tick;
                 i < insert_at_tick_id;
                 i += 1)
            {
                Object *obj = Client_SnapshotObjectAtTick(snap, i);
                if (Object_IsInit(obj))
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
        Object *insert_at_obj = Client_SnapshotObjectAtTick(snap, insert_at_tick_id);
        *insert_at_obj = insert_obj;
    }

    Assert(snap->oldest_server_tick >= minimum_server_tick);
    return false; // no error
}

static Tick_Input *Client_PollInput(AppState *app)
{
    V2 dir = {0};
    if (app->keyboard[SDL_SCANCODE_W] || app->keyboard[SDL_SCANCODE_UP])    dir.y += 1;
    if (app->keyboard[SDL_SCANCODE_S] || app->keyboard[SDL_SCANCODE_DOWN])  dir.y -= 1;
    if (app->keyboard[SDL_SCANCODE_A] || app->keyboard[SDL_SCANCODE_LEFT])  dir.x -= 1;
    if (app->keyboard[SDL_SCANCODE_D] || app->keyboard[SDL_SCANCODE_RIGHT]) dir.x += 1;

    Tick_Input *input = Q_Push(app->client.inputs_qbuf, &app->client.inputs_range);
    SDL_zerop(input);

    input->move_dir = V2_Normalize(dir);
    if (app->pathing_marker_set)
    {
        app->pathing_marker_set = false;
        input->is_pathing = true;
        input->pathing_world_p = Object_Get(app, app->pathing_marker, ObjCategory_Local)->p;
    }

    return input;
}
