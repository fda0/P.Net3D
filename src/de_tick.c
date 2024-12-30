//
// Physics update tick
// @todo this should run on a separate thread
//
static Tick_Input *Tick_PollInput(AppState *app)
{
    // select slot from circular buffer
    Uint64 current = app->tick_input_max % ArrayCount(app->tick_input_buf);
    {
        app->tick_input_max += 1;
        Uint64 max = app->tick_input_max % ArrayCount(app->tick_input_buf);
        Uint64 min = app->tick_input_min % ArrayCount(app->tick_input_buf);
        if (min == max)
            app->tick_input_min += 1;
    }

    Tick_Input *input = app->tick_input_buf + current;

    V2 dir = {0};
    if (app->keyboard[SDL_SCANCODE_W] || app->keyboard[SDL_SCANCODE_UP])    dir.y += 1;
    if (app->keyboard[SDL_SCANCODE_S] || app->keyboard[SDL_SCANCODE_DOWN])  dir.y -= 1;
    if (app->keyboard[SDL_SCANCODE_A] || app->keyboard[SDL_SCANCODE_LEFT])  dir.x -= 1;
    if (app->keyboard[SDL_SCANCODE_D] || app->keyboard[SDL_SCANCODE_RIGHT]) dir.x += 1;
    input->move_dir = V2_Normalize(dir);

    return input;
}

static void Tick_AdvanceSimulation(AppState *app)
{
    Tick_Input *input = Tick_PollInput(app);

    // update prev_p
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        obj->prev_p = obj->p;
    }

    // player input
    {
        Object *player = Object_FromNetSlot(app, app->player_network_slot);
        if (!Object_IsZero(app, player))
        {
            float player_speed = 200.f * TIME_STEP;
            player->dp = V2_Scale(input->move_dir, player_speed);
        }
        player->some_number += 1;
    }

    // movement & collision
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        obj->has_collision = false;
    }
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        if (!(obj->flags & ObjectFlag_Move)) continue;
        Sprite *obj_sprite = Sprite_Get(app, obj->sprite_id);

        obj->p = V2_Add(obj->p, obj->dp);

        ForU32(collision_iteration, 8) // support up to 8 overlapping wall collisions
        {
            float closest_obstacle_separation_dist = FLT_MAX;
            V2 closest_obstacle_wall_normal = {0};

            Col_Vertices obj_verts = obj_sprite->collision_vertices;
            Vertices_Offset(obj_verts.arr, ArrayCount(obj_verts.arr), obj->p);
            V2 obj_center = Vertices_Average(obj_verts.arr, ArrayCount(obj_verts.arr));

            ForU32(obstacle_id, app->object_count)
            {
                Object *obstacle = app->object_pool + obstacle_id;
                if (!(obstacle->flags & ObjectFlag_Collide)) continue;
                if (obj == obstacle) continue;
                Sprite *obstacle_sprite = Sprite_Get(app, obstacle->sprite_id);

                Col_Vertices obstacle_verts = obstacle_sprite->collision_vertices;
                Vertices_Offset(obstacle_verts.arr, ArrayCount(obstacle_verts.arr), obstacle->p);
                V2 obstacle_center = Vertices_Average(obstacle_verts.arr, ArrayCount(obstacle_verts.arr));

                float biggest_dist = -FLT_MAX;
                V2 wall_normal = {0};

                // @info(mg) SAT algorithm needs 2 iterations
                // from the perspective of the obj
                // and from the perspective of the obstacle.
                ForU32(sat_iteration, 2)
                {
                    bool use_obj_normals = !sat_iteration;
                    Col_Normals normals = (use_obj_normals ?
                                           obj_sprite->collision_normals :
                                           obstacle_sprite->collision_normals);

                    Col_Projection proj_obj = CollisionProjection(normals, obj_verts);
                    Col_Projection proj_obstacle = CollisionProjection(normals, obstacle_verts);

                    ForArray(i, proj_obj.arr)
                    {
                        static_assert(ArrayCount(proj_obj.arr) == ArrayCount(normals.arr));
                        V2 normal = normals.arr[i];

                        V2 obstacle_dir = V2_Sub(obstacle_center, obj_center);
                        if (V2_Inner(normal, obstacle_dir) < 0)
                        {
                            continue;
                        }

                        float d = RngF_MaxDistance(proj_obj.arr[i], proj_obstacle.arr[i]);
                        if (d > 0.f)
                        {
                            // @info(mg) We can exit early from checking this
                            //     obstacle since we found an axis that has
                            //     a separation between obj and obstacle.
                            goto skip_this_obstacle;
                        }

                        if (d > biggest_dist)
                        {
                            biggest_dist = d;
                            wall_normal = normal;

                            if (use_obj_normals || true) // @todo(mg) I have a bug somewhere? Why does `|| true` fix my bug?
                            {
                                // @note if we pick obj's normal we need to
                                // inverse it if we want to move out of the obstacle
                                wall_normal = V2_Reverse(wall_normal);
                            }
                        }
                    }
                }

                if (closest_obstacle_separation_dist > biggest_dist)
                {
                    closest_obstacle_separation_dist = biggest_dist;
                    closest_obstacle_wall_normal = wall_normal;
                }

                obj->has_collision |= (biggest_dist < 0.f);
                obstacle->has_collision |= (biggest_dist < 0.f);

                skip_this_obstacle:;
            }

            if (closest_obstacle_separation_dist < 0.f)
            {
                V2 move_out_dir = closest_obstacle_wall_normal;
                float move_out_magnitude = -closest_obstacle_separation_dist;

                V2 move_out = V2_Scale(move_out_dir, move_out_magnitude);
                obj->p = V2_Add(obj->p, move_out);

                // remove all velocity on collision axis
                // we might want to do something different here!
                if (move_out.x) obj->dp.x = 0;
                if (move_out.y) obj->dp.y = 0;
            }
            else
            {
                // Collision not found, stop iterating
                break;
            }
        } // collision_iteration
    } // obj_id


    // animate textures
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        if (Sprite_Get(app, obj->sprite_id)->tex_frames <= 1) continue;

        Uint32 frame_index_map[8] =
        {
            0, 1, 2, 1,
            0, 3, 4, 3
        };

        bool in_idle_frame = (0 == frame_index_map[obj->sprite_animation_index]);

        float distance = V2_Length(V2_Sub(obj->p, obj->prev_p));
        float anim_speed = (16.f * TIME_STEP);
        anim_speed += (5.f * distance * TIME_STEP);

        if (!distance && in_idle_frame)
        {
            anim_speed = 0.f;
        }
        obj->sprite_animation_t += anim_speed;

        float period = 1.f;
        while (obj->sprite_animation_t > period)
        {
            obj->sprite_animation_t -= period;
            obj->sprite_animation_index += 1;
        }

        obj->sprite_animation_index %= ArrayCount(frame_index_map);
        obj->sprite_frame_index = frame_index_map[obj->sprite_animation_index];
    }


    // save networked objects state
    if (Net_IsServer(app))
    {
        Uint64 snap_index = app->server.next_tick % ArrayCount(app->server.snaps);
        app->server.next_tick += 1;
        ServerTickSnapshot *snap = app->server.snaps + snap_index;

        static_assert(ArrayCount(snap->objs) == ArrayCount(app->network_ids));
        ForArray(i, snap->objs)
        {
            Object *dst = snap->objs + i;
            Object *src = Object_FromNetSlot(app, i);
            memcpy(dst, src, sizeof(Object));
        }
    }
}

static Object Object_Lerp(Object prev, Object next, float t)
{
    Object result = prev;

    if (1)
    {
        result.p = V2_Lerp(prev.p, next.p, t);
        result.dp = V2_Lerp(prev.dp, next.dp, t);
        result.prev_p = V2_Lerp(prev.prev_p, next.prev_p, t);
        result.sprite_color = ColorF_Lerp(prev.sprite_color, next.sprite_color, t);
        result.sprite_animation_t = LerpF(prev.sprite_animation_t, next.sprite_animation_t, t);

        result.sprite_animation_index = (Uint32)RoundF(LerpF((float)prev.sprite_animation_index,
                                                             (float)next.sprite_animation_index, t));
        result.sprite_frame_index = (Uint32)RoundF(LerpF((float)prev.sprite_frame_index,
                                                         (float)next.sprite_frame_index, t));
        result.has_collision = (Uint32)RoundF(LerpF((float)prev.has_collision,
                                                    (float)next.has_collision, t));
    }

    return result;
}

static Object Client_LerpNetObject(AppState *app, Uint64 net_slot, Uint64 tick_id)
{
    Assert(net_slot < ArrayCount(app->client.obj_snaps));
    Client_Snapshot *snap = app->client.obj_snaps + net_slot;

    Assert(tick_id <= snap->latest_server_tick &&
           tick_id >= snap->oldest_server_tick);

    Object *exact_obj = Client_SnapshotObjectAtTick(snap, tick_id);
    if (exact_obj->flags) // exact object found
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

    // do the interpolation
    Uint64 id_range = next_id - prev_id;
    Uint64 id_offset = tick_id - prev_id;
    float t = (float)id_offset / (float)id_range;
    Object result = Object_Lerp(*prev, *next, t);
    return result;
}

static void Tick_Playback(AppState *app)
{
    Uint64 smallest_latest_server_tick = ~0ull;
    Uint64 biggest_oldest_server_tick = 0;
    ForArray(obj_i, app->client.obj_snaps)
    {
        Client_Snapshot *snap = app->client.obj_snaps + obj_i;
        if (snap->latest_server_tick < smallest_latest_server_tick)
        {
            smallest_latest_server_tick = snap->latest_server_tick;
        }
        if (snap->oldest_server_tick > biggest_oldest_server_tick)
        {
            biggest_oldest_server_tick = snap->oldest_server_tick;
        }
    }

    if (biggest_oldest_server_tick > app->client.next_playback_tick)
    {
        SDL_Log("%s: Server is ahead of the client; "
                "client's next_playback_tick: %llu, "
                "biggest_oldest_server_tick: %llu, "
                "bumping client's next_playback_tick",
                Net_Label(app),
                app->client.next_playback_tick,
                biggest_oldest_server_tick);

        app->client.next_playback_tick = biggest_oldest_server_tick;
    }

    if (smallest_latest_server_tick < app->client.next_playback_tick)
    {
        SDL_Log("%s: Ran out of tick playback state; "
                "next_playback_tick: %llu, "
                "smallest_latest_server_tick: %llu, "
                "tick_bump_correction: %llu",
                Net_Label(app),
                app->client.next_playback_tick,
                smallest_latest_server_tick,
                app->client.tick_bump_correction);
        return;
    }

    ForArray(net_slot, app->client.obj_snaps)
    {
        Object *net_obj = Object_FromNetSlot(app, net_slot);
        Object interpolated = Client_LerpNetObject(app, net_slot, app->client.next_playback_tick);
        *net_obj = interpolated;
    }
    app->client.next_playback_tick += 1;
}

static void Tick_Iterate(AppState *app)
{
    if (Net_IsServer(app))
    {
        Tick_AdvanceSimulation(app);
    }

    if (Net_IsClient(app))
    {
        Tick_Playback(app);

        if (app->client.tick_bump_correction)
        {
            Tick_Playback(app);
            app->client.tick_bump_correction -= 1;
        }
    }
}
