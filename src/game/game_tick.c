//
// Physics update tick
// @todo this should run on a separate thread
//
static Tick_Input Tick_NormalizeInput(Tick_Input input)
{
    input.move_dir = V2_Normalize(V2_NanToZero(input.move_dir));
    return input;
}

static void Tick_AdvanceSimulation()
{
    // update prev_p
    ForArray(obj_index, APP.all_objects)
    {
        Object *obj = APP.all_objects + obj_index;
        if (Obj_HasAnyFlag(obj, ObjectFlag_Move|ObjectFlag_AnimateRotation))
        {
            obj->s.prev_p = obj->s.p;
        }
    }

    // apply player input
    ForArray(player_index, APP.server.player_keys)
    {
        Obj_Key player_key = APP.server.player_keys[player_index];
        Object *player = Obj_Get(player_key, ObjCategory_Net);
        if (Obj_IsNil(player))
            continue;

        Tick_Input input = Server_GetPlayerInput(player_index);

        if (input.is_pathing)
        {
            player->s.is_pathing = true;
            player->s.pathing_dest_p = input.pathing_world_p;
        }
        if (input.move_dir.x || input.move_dir.y)
        {
            player->s.is_pathing = false;
        }

        V2 player_dir = input.move_dir;
        if (player->s.is_pathing)
        {
            V2 dir = V2_Sub(player->s.pathing_dest_p, player->s.p);
            float len_sq = V2_LengthSq(dir);
            if (len_sq > 1.f)
            {
                float len_inv = InvSqrtF(len_sq);
                dir = V2_Scale(dir, len_inv);
                player_dir = dir;
            }
            else
            {
                player->s.is_pathing = false;
            }
        }

        float player_speed = 60.f * TIME_STEP;
        player->s.dp = V2_Scale(player_dir, player_speed);
        player->s.some_number += 1;
    }

    // movement & collision
    ForArray(obj_index, APP.all_objects)
    {
        Object *obj = APP.all_objects + obj_index;
        if (!Obj_HasAnyFlag(obj, ObjectFlag_Move|ObjectFlag_Collide)) continue;

        // reset debug flag
        obj->s.did_collide = false;
    }

    ForArray(obj_index, APP.all_objects)
    {
        Object *obj = APP.all_objects + obj_index;
        if (!Obj_HasAnyFlag(obj, ObjectFlag_Move)) continue;

        // move object
        obj->s.p = V2_Add(obj->s.p, obj->s.dp);

        // check collision
        ForU32(collision_iteration, 8) // support up to 8 overlapping wall collisions
        {
            float closest_obstacle_separation_dist = FLT_MAX;
            V2 closest_obstacle_wall_normal = {0};

            CollisionVertices obj_verts = obj->s.collision.verts;
            Vertices_Offset(obj_verts.arr, ArrayCount(obj_verts.arr), obj->s.p);
            V2 obj_center = Vertices_Average(obj_verts.arr, ArrayCount(obj_verts.arr));

            ForArray(obstacle_index, APP.all_objects)
            {
                Object *obstacle = APP.all_objects + obstacle_index;
                if (!Obj_HasAnyFlag(obstacle, ObjectFlag_Collide)) continue;
                if (obj == obstacle) continue;

                CollisionVertices obstacle_verts = obstacle->s.collision.verts;
                Vertices_Offset(obstacle_verts.arr, ArrayCount(obstacle_verts.arr), obstacle->s.p);
                V2 obstacle_center = Vertices_Average(obstacle_verts.arr, ArrayCount(obstacle_verts.arr));

                float biggest_dist = -FLT_MAX;
                V2 wall_normal = {0};

                // @info(mg) SAT algorithm needs 2 iterations
                // from the perspective of the obj
                // and from the perspective of the obstacle.
                ForU32(sat_iteration, 2)
                {
                    bool use_obj_normals = !sat_iteration;
                    CollisionNormals normals = (use_obj_normals ?
                                                obj->s.collision.norms :
                                                obstacle->s.collision.norms);

                    CollisionProjection proj_obj = Collision_CalculateProjection(normals, obj_verts);
                    CollisionProjection proj_obstacle = Collision_CalculateProjection(normals, obstacle_verts);

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

                obj->s.did_collide |= (biggest_dist < 0.f);
                obstacle->s.did_collide |= (biggest_dist < 0.f);

                skip_this_obstacle:;
            }

            if (closest_obstacle_separation_dist < 0.f)
            {
                V2 move_out_dir = closest_obstacle_wall_normal;
                float move_out_magnitude = -closest_obstacle_separation_dist;

                V2 move_out = V2_Scale(move_out_dir, move_out_magnitude);
                obj->s.p = V2_Add(obj->s.p, move_out);

                // remove all velocity on collision axis
                // we might want to do something different here!
                if (move_out.x) obj->s.dp.x = 0;
                if (move_out.y) obj->s.dp.y = 0;
            }
            else
            {
                // Collision not found, stop iterating
                break;
            }
        } // collision_iteration
    } // obj_index
}

static Obj_Sync Obj_SyncLerp(Obj_Sync prev, Obj_Sync next, float t)
{
    Obj_Sync res = prev;
    res.p = V2_Lerp(prev.p, next.p, t);
    res.dp = V2_Lerp(prev.dp, next.dp, t);
    res.prev_p = V2_Lerp(prev.prev_p, next.prev_p, t);
    res.color = ColorF_Lerp(prev.color, next.color, t);
    res.rot_z = LerpF(prev.rot_z, next.rot_z, t);
    res.did_collide = (bool)RoundF(LerpF((float)prev.did_collide, (float)next.did_collide, t));
    return res;
}

static void Tick_Playback()
{
    U64 smallest_latest_server_tick = ~0ull;
    U64 biggest_oldest_server_tick = 0;
    ForArray(obj_i, APP.client.obj_snaps)
    {
        Client_Snapshot *snap = APP.client.obj_snaps + obj_i;
        if (snap->latest_server_tick < smallest_latest_server_tick)
        {
            smallest_latest_server_tick = snap->latest_server_tick;
        }
        if (snap->oldest_server_tick > biggest_oldest_server_tick)
        {
            biggest_oldest_server_tick = snap->oldest_server_tick;
        }
    }

    if (biggest_oldest_server_tick > APP.client.next_playback_tick)
    {
        LOG(LogFlags_NetTick,
            "%s: Server is too ahead from the client; "
            "biggest_oldest_server_tick: %llu, "
            "smallest_latest_server_tick: %llu, "
            "client's next_playback_tick: %llu, "
            "playback delay: %d, "
            "playback catchup %d, "
            "[bumping client's next_playback_tick]",
            Net_Label(),
            biggest_oldest_server_tick,
            smallest_latest_server_tick,
            APP.client.next_playback_tick,
            (int)APP.client.current_playback_delay,
            (int)APP.client.playable_tick_deltas.tick_catchup);

        APP.client.next_playback_tick = biggest_oldest_server_tick;
    }

    if (smallest_latest_server_tick < APP.client.next_playback_tick)
    {
        LOG(LogFlags_NetTick,
            "%s: Ran out of tick playback state; "
            "next_playback_tick: %llu, "
            "smallest_latest_server_tick: %llu, "
            "playback delay: %d, "
            "playback catchup %d",
            Net_Label(),
            APP.client.next_playback_tick,
            smallest_latest_server_tick,
            (int)APP.client.current_playback_delay,
            (int)APP.client.playable_tick_deltas.tick_catchup);
        return;
    }

    // calc current delay
    {
        U64 current_playback_delay_u64 = smallest_latest_server_tick - APP.client.next_playback_tick;
        APP.client.current_playback_delay = Saturate_U64toU16(current_playback_delay_u64);
    }

    if (TickDeltas_AddTick(&APP.client.playable_tick_deltas, smallest_latest_server_tick))
    {
        TickDeltas_UpdateCatchup(&APP.client.playable_tick_deltas, APP.client.current_playback_delay);
        if (APP.client.playable_tick_deltas.tick_catchup)
        {
            LOG(LogFlags_NetCatchup,
                "%s: Current playback delay: %d, "
                " Setting playback catchup to %d",
                Net_Label(),
                (int)APP.client.current_playback_delay,
                (int)APP.client.playable_tick_deltas.tick_catchup);
        }
    }

    ForU32(net_index, OBJ_MAX_NETWORK_OBJECTS)
    {
        Obj_Sync interpolated_sync = Client_LerpNetObject(net_index, APP.client.next_playback_tick);
        Object *net_obj = Obj_FromNetIndex(net_index);
        net_obj->s = interpolated_sync;
    }
    APP.client.next_playback_tick += 1;
}

static void Tick_Iterate()
{
    if (Net_IsServer())
    {
        Tick_AdvanceSimulation();
    }

    if (Net_IsClient())
    {
        Tick_Playback();
        if (APP.client.playable_tick_deltas.tick_catchup > 0)
        {
            APP.client.playable_tick_deltas.tick_catchup -= 1;
            Tick_Playback();
        }
    }
}
