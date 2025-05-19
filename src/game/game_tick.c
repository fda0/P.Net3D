//
// Physics update tick
// @todo this should run on a separate thread
//
static TICK_Input TICK_NormalizeInput(TICK_Input input)
{
  input.move_dir = V2_Normalize(V2_NanToZero(input.move_dir));
  return input;
}

static void TICK_AdvanceSimulation()
{
  // apply player input
  ForArray32(player_index, APP.server.player_keys)
  {
    OBJ_Key player_key = APP.server.player_keys[player_index];
    Object *player = OBJ_Get(player_key, OBJ_Network);
    if (OBJ_IsNil(player))
      continue;

    TICK_Input input = SERVER_GetPlayerInput(player_index);

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
      V2 player_to_dest = V2_Sub(player->s.pathing_dest_p, V2_FromV3_XY(player->s.p));
      float len_sq = V2_LengthSq(player_to_dest);
      if (len_sq > (0.01f * 0.01f))
      {
        player_dir = V2_Scale(player_to_dest, FInvSqrt(len_sq));
      }
      else
      {
        player->s.is_pathing = false;
      }
    }

    float player_speed = 0.6f * TICK_FLOAT_STEP;
    player->s.desired_dp = V3_From_XY_Z(V2_Scale(player_dir, player_speed), 0);
  }

  ForArray(obj_index, APP.all_objects)
  {
    Object *obj = APP.all_objects + obj_index;
    if (!OBJ_HasAnyFlag(obj, ObjFlag_Move)) continue;
    OBJ_UpdateColliderNormals(obj);

    // movement simulation
    V2 obj_dp = V2_FromV3_XY(obj->s.desired_dp);
    V2 obj_pos = V2_Add(V2_FromV3_XY(obj->s.p), obj_dp); // move obj

    // check collision
    ForU32(collision_iteration, 8) // support up to 8 overlapping wall collisions
    {
      float closest_obstacle_separation_dist = FLT_MAX;
      V2 closest_obstacle_wall_normal = {0};

      OBJ_Collider obj_verts = obj->s.collider_vertices;
      OBJ_OffsetColliderVertices(&obj_verts, obj_pos);

      ForArray(obstacle_index, APP.all_objects)
      {
        Object *obstacle = APP.all_objects + obstacle_index;
        if (!OBJ_HasAnyFlag(obstacle, ObjFlag_Collide)) continue;
        if (obj == obstacle) continue;
        OBJ_UpdateColliderNormals(obstacle);

        V2 obstacle_pos = V2_FromV3_XY(obstacle->s.p);
        OBJ_Collider obstacle_verts = obstacle->s.collider_vertices;
        OBJ_OffsetColliderVertices(&obstacle_verts, obstacle_pos);

        float biggest_dist = -FLT_MAX;
        V2 wall_normal = {0};

        // @info(mg) SAT algorithm needs 2 iterations
        // from the perspective of the obj
        // and from the perspective of the obstacle.
        ForU32(sat_iteration, 2)
        {
          bool use_obj_normals = !sat_iteration;
          OBJ_Collider normals = (use_obj_normals ?
                                  obj->l.collider_normals :
                                  obstacle->l.collider_normals);

          OBJ_ColliderProjection proj_obj = OBJ_CalculateColliderProjection(&normals, &obj_verts);
          OBJ_ColliderProjection proj_obstacle = OBJ_CalculateColliderProjection(&normals, &obstacle_verts);

          ForArray(i, proj_obj.ranges)
          {
            static_assert(ArrayCount(proj_obj.ranges) == ArrayCount(normals.vals));
            V2 normal = normals.vals[i];

            V2 obstacle_dir = V2_Sub(obstacle_pos, obj_pos);
            if (V2_Dot(normal, obstacle_dir) < 0)
            {
              continue;
            }

            float d = RngF_MaxDistance(proj_obj.ranges[i], proj_obstacle.ranges[i]);
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
              wall_normal = V2_Reverse(normal);
            }
          }
        }

        if (closest_obstacle_separation_dist > biggest_dist)
        {
          closest_obstacle_separation_dist = biggest_dist;
          closest_obstacle_wall_normal = wall_normal;
        }

        skip_this_obstacle:;
      }

      if (closest_obstacle_separation_dist < 0.f)
      {
        V2 move_out_dir = closest_obstacle_wall_normal;
        float move_out_magnitude = -closest_obstacle_separation_dist;

        V2 move_out = V2_Scale(move_out_dir, move_out_magnitude);
        obj_pos = V2_Add(obj_pos, move_out);

        // remove all velocity on collision axis
        // we might want to do something different here!
        if (move_out.x) obj_dp.x = 0;
        if (move_out.y) obj_dp.y = 0;
      }
      else
      {
        // Collision not found, stop iterating
        break;
      }
    } // collision_iteration

    V3 prev_pos = obj->s.p;
    obj->s.p.x = obj_pos.x;
    obj->s.p.y = obj_pos.y;
    obj->s.moved_dp = V3_Sub(obj->s.p, prev_pos);
  } // obj_index

  ForArray(obj_index, APP.all_objects)
  {
    Object *obj = APP.all_objects + obj_index;
    if (OBJ_HasAnyFlag(obj, ObjFlag_AnimateT))
    {
      // set walking animation if player moves
      if (V3_HasLength(obj->s.desired_dp))
      {
        obj->s.animation_index = 22;
      }
    }

    if (OBJ_HasAllFlags(obj, ObjFlag_AnimateRotation))
    {
      if (V3_HasLength(obj->s.moved_dp))
      {
        V2 dir = V2_Normalize(V2_FromV3_XY(obj->s.moved_dp));
        float rot = -FAtan2(dir) + 0.25f;
        rot = FWrap(-0.5f, 0.5f, rot);
        obj->s.rotation = Quat_FromAxisAngle_RH((V3){0,0,1}, rot);
      }
    }
  }
}

static OBJ_Sync OBJ_SyncLerp(OBJ_Sync prev, OBJ_Sync next, float t)
{
  OBJ_Sync res = prev;
  res.p = V3_Lerp(prev.p, next.p, t);
  res.desired_dp = V3_Lerp(prev.desired_dp, next.desired_dp, t);
  res.moved_dp = V3_Lerp(prev.moved_dp, next.moved_dp, t);
  res.color = Color32_Lerp(prev.color, next.color, t);
  res.rotation = Quat_SLerp(prev.rotation, next.rotation, t);
  return res;
}

static void TICK_Playback()
{
  U64 smallest_latest_server_tick = ~0ull;
  U64 biggest_oldest_server_tick = 0;
  ForArray(obj_i, APP.client.snaps_of_objs)
  {
    CLIENT_ObjSnapshots *snap = APP.client.snaps_of_objs + obj_i;
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
    LOG(LOG_NetTick,
        "%s: Server is too ahead from the client; "
        "biggest_oldest_server_tick: %llu, "
        "smallest_latest_server_tick: %llu, "
        "client's next_playback_tick: %llu, "
        "playback delay: %d, "
        "playback catchup %d, "
        "[bumping client's next_playback_tick]",
        NET_Label(),
        biggest_oldest_server_tick,
        smallest_latest_server_tick,
        APP.client.next_playback_tick,
        (int)APP.client.current_playback_delay,
        (int)APP.client.playable_tick_deltas.tick_catchup);

    APP.client.next_playback_tick = biggest_oldest_server_tick;
  }

  if (smallest_latest_server_tick < APP.client.next_playback_tick)
  {
    LOG(LOG_NetTick,
        "%s: Ran out of tick playback state; "
        "next_playback_tick: %llu, "
        "smallest_latest_server_tick: %llu, "
        "playback delay: %d, "
        "playback catchup %d",
        NET_Label(),
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
      LOG(LOG_NetCatchup,
          "%s: Current playback delay: %d, "
          " Setting playback catchup to %d",
          NET_Label(),
          (int)APP.client.current_playback_delay,
          (int)APP.client.playable_tick_deltas.tick_catchup);
    }
  }

  ForU32(net_index, OBJ_MAX_NETWORK_OBJECTS)
  {
    OBJ_Sync interpolated_sync = CLIENT_LerpObjSync(net_index, APP.client.next_playback_tick);
    Object *net_obj = OBJ_FromNetIndex(net_index);
    net_obj->s = interpolated_sync;
  }
  APP.client.next_playback_tick += 1;
}

static void TICK_Iterate()
{
  if (NET_IsServer())
  {
    TICK_AdvanceSimulation();
  }

  if (NET_IsClient())
  {
    TICK_Playback();
    if (APP.client.playable_tick_deltas.tick_catchup > 0)
    {
      APP.client.playable_tick_deltas.tick_catchup -= 1;
      TICK_Playback();
    }
  }
}
