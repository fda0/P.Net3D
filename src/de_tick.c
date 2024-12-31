//
// Physics update tick
// @todo this should run on a separate thread
//
static Tick_Input Tick_NormalizeInput(Tick_Input input)
{
    input.move_dir = V2_Normalize(V2_ZeroOutNan(input.move_dir));
    return input;
}

static void Tick_AdvanceSimulation(AppState *app)
{
    // update prev_p
    ForArray(obj_index, app->all_objects)
    {
        Object *obj = app->all_objects + obj_index;
        if (!Object_HasAnyFlag(obj, ObjectFlag_Move)) continue;

        obj->prev_p = obj->p;
    }

    // apply player input
    ForArray(player_index, app->server.player_keys)
    {
        Tick_Input input = Server_PopPlayerInput(app, player_index);

        Object_Key player_key = app->server.player_keys[player_index];
        Object *player = Object_Get(app, player_key, ObjCategory_Net);
        if (Object_IsNil(player))
            continue;

        float player_speed = 200.f * TIME_STEP;
        player->dp = V2_Scale(input.move_dir, player_speed);
        player->some_number += 1;
    }

    // movement & collision
    ForArray(obj_index, app->all_objects)
    {
        Object *obj = app->all_objects + obj_index;
        if (!Object_HasAnyFlag(obj, ObjectFlag_Move|ObjectFlag_Collide)) continue;

        // reset debug flag
        obj->has_collision = false;
    }

    ForArray(obj_index, app->all_objects)
    {
        Object *obj = app->all_objects + obj_index;
        if (!Object_HasAnyFlag(obj, ObjectFlag_Move)) continue;
        Sprite *obj_sprite = Sprite_Get(app, obj->sprite_id);

        // move object
        obj->p = V2_Add(obj->p, obj->dp);

        // check collision
        ForU32(collision_iteration, 8) // support up to 8 overlapping wall collisions
        {
            float closest_obstacle_separation_dist = FLT_MAX;
            V2 closest_obstacle_wall_normal = {0};

            Col_Vertices obj_verts = obj_sprite->collision_vertices;
            Vertices_Offset(obj_verts.arr, ArrayCount(obj_verts.arr), obj->p);
            V2 obj_center = Vertices_Average(obj_verts.arr, ArrayCount(obj_verts.arr));

            ForArray(obstacle_index, app->all_objects)
            {
                Object *obstacle = app->all_objects + obj_index;
                if (!Object_HasAnyFlag(obj, ObjectFlag_Collide)) continue;
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
    } // obj_index


    // animate textures
    ForArray(obj_index, app->all_objects)
    {
        Object *obj = app->all_objects + obj_index;
        if (!Object_HasAnyFlag(obj, ObjectFlag_Draw)) continue;
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
        LOG(LogFlags_NetTick,
            "%s: Server is too ahead from the client; "
            "biggest_oldest_server_tick: %llu, "
            "smallest_latest_server_tick: %llu, "
            "client's next_playback_tick: %llu, "
            "playback delay: %llu, "
            "playback catchup %d, "
            "[bumping client's next_playback_tick]",
            Net_Label(app),
            biggest_oldest_server_tick,
            smallest_latest_server_tick,
            app->client.next_playback_tick,
            app->client.current_playback_delay,
            (int)app->client.playback_tick_catchup);

        app->client.next_playback_tick = biggest_oldest_server_tick;
    }

    if (smallest_latest_server_tick < app->client.next_playback_tick)
    {
        LOG(LogFlags_NetTick,
            "%s: Ran out of tick playback state; "
            "next_playback_tick: %llu, "
            "smallest_latest_server_tick: %llu, "
            "playback delay: %llu, "
            "playback catchup %d",
            Net_Label(app),
            app->client.next_playback_tick,
            smallest_latest_server_tick,
            app->client.current_playback_delay,
            (int)app->client.playback_tick_catchup);
        return;
    }

    // playback delay catchup calculations
    {
        app->client.current_playback_delay = smallest_latest_server_tick - app->client.next_playback_tick;

        if (app->client.prev_smallest_latest_server_tick !=
            smallest_latest_server_tick)
        {
            // save the delta
            Sint64 delta = smallest_latest_server_tick - app->client.prev_smallest_latest_server_tick;
            app->client.prev_smallest_latest_server_tick = smallest_latest_server_tick;

            app->client.latest_deltas[app->client.latest_delta_index] = delta;
            app->client.latest_delta_index = (app->client.latest_delta_index + 1) % ArrayCount(app->client.latest_deltas);

            // find biggest recent delta
            Sint16 biggest_delta = 0;
            ForArray(i, app->client.latest_deltas)
            {
                if (app->client.latest_deltas[i] > biggest_delta)
                    biggest_delta = app->client.latest_deltas[i];
            }

            Sint16 target_playback_delay = biggest_delta + biggest_delta/64 + 1;
            if (target_playback_delay < app->client.current_playback_delay)
            {
                app->client.playback_tick_catchup = app->client.current_playback_delay - target_playback_delay;

                LOG(LogFlags_NetCatchup,
                    "%s: Current playback delay: %llu, "
                    "Target playback delay: %d, "
                    " Setting playback catchup to %d, ",
                    Net_Label(app),
                    app->client.current_playback_delay,
                    (int)target_playback_delay,
                    (int)app->client.playback_tick_catchup);
            }
            else
            {
                app->client.playback_tick_catchup = 0;
            }
        }
    }

    ForU32(net_index, OBJ_MAX_NETWORK_OBJECTS)
    {
        Object *net_obj = Object_FromNetIndex(app, net_index);
        Object interpolated = Client_LerpNetObject(app, net_index, app->client.next_playback_tick);
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
        Client_PollInput(app);

        Tick_Playback(app);
        if (app->client.playback_tick_catchup > 0)
        {
            app->client.playback_tick_catchup -= 1;
            Tick_Playback(app);
        }
    }
}
