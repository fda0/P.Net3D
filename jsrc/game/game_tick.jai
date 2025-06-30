TICK_RATE :: 100;
TICK_TIMESTAMP_STEP :: 1000 / TICK_RATE;
TICK_FLOAT_STEP :: 1.0 / TICK_RATE;

TICK_Iterate :: ()
{
  #assert((1000 % TICK_TIMESTAMP_STEP) == 0);
  #assert((1000 / TICK_TIMESTAMP_STEP) == TICK_RATE);
  tick_count := G.tick_timestamp_accumulator / TICK_TIMESTAMP_STEP;
  G.tick_timestamp_accumulator -= tick_count * TICK_TIMESTAMP_STEP;

  for MakeRange(tick_count)
  {
    G.tick_number += 1;

    if NET_IsServer()
      TICK_AdvanceSimulation();

    if NET_IsClient()
    {
      TICK_Playback();
      if G.client.playable_tick_deltas.tick_catchup > 0
      {
        G.client.playable_tick_deltas.tick_catchup -= 1;
        TICK_Playback();
      }
    }
  }
}

ATTACK_LANDED_T :: 0.25;
ATTACK_COOLDOWN_T :: 1.0;

TICK_AdvanceSimulation :: ()
{
  // apply player action
  for player_key, player_index: G.server.player_keys
  {
    player := OBJ_Get(player_key, .NETWORK);
    if OBJ_IsNil(player) continue;

    player_move_dir: V2;
    action := SERVER_GetPlayerAction(xx player_index);

    if action.type == .PATHING ||
       action.type == .PATHING_DIRECTION
    {
      player_to_destination := action.world_p.xy - player.s.p.xy;
      distance := length(player_to_destination);
      if distance > 0.05 then player_move_dir = player_to_destination * (1.0 / distance);
    }

    not_attacking := action.type != .ATTACK;
    player.s.attack_t += TICK_FLOAT_STEP;

    if player.s.attack_t >= 0.0
    {
      player.s.hide_attack_animation_tail = false;
    }

    if action.type == .ATTACK
    {
      attack_object := OBJ_Get(action.target_object, .NETWORK);
      if !OBJ_IsNil(attack_object)
      {
        player_to_destination := attack_object.s.p.xy - player.s.p.xy;
        distance := length(player_to_destination);

        if distance < 1
        {
          player.s.is_attacking = true;

          attack_damage := 10.0;
          while player.s.attack_t >= ATTACK_LANDED_T
          {
            player.s.attack_t -= ATTACK_COOLDOWN_T;
            attack_object.s.hp -= attack_damage;
          }
        }
        else if distance > 0.05
        {
          player_move_dir = player_to_destination * (1.0 / distance);
          not_attacking = true;
        }
      }
    }

    if not_attacking
    {
      player.s.is_attacking = false;
      player.s.hide_attack_animation_tail = true;
      player.s.attack_t = min(player.s.attack_t, 0.0);
    }

    player_speed := 1.4 * TICK_FLOAT_STEP;
    player.s.desired_dp = V3.{xy = player_move_dir * player_speed, z = 0};
  }

  for * obj: G.obj.all_objects
  {
    if !OBJ_HasAnyFlag(obj, .MOVE) continue;

    // movement simulation
    obj_dp := obj.s.desired_dp.xy;
    obj_pos := obj.s.p.xy + obj_dp; // move obj

    // check collision
    for collision_iteration: 0..7 // support up to 8 overlapping wall collisions
    {
      closest_obstacle_separation_dist := FLOAT32_MAX;
      closest_obstacle_wall_normal: V2;

      obj_collider := obj.s.collider;
      OBJ_OffsetCollider(*obj_collider, obj_pos);

      for * obstacle: G.obj.all_objects
      {
        if obj == obstacle continue;
        if !OBJ_HasAnyFlag(obstacle, .COLLIDE) continue;

        obstacle_pos := obstacle.s.p.xy;
        obstacle_collider := obstacle.s.collider;
        OBJ_OffsetCollider(*obstacle_collider, obstacle_pos);

        biggest_dist := -FLOAT32_MAX;
        wall_normal: V2;

        // @info(mg) SAT algorithm needs 2 iterations
        // from the perspective of the obj
        // and from the perspective of the obstacle.
        for sat_iteration: 0..1
        {
          normal_source := ifx sat_iteration == 0 then obstacle_collider else obj_collider;

          projection_obj := OBJ_CalculateColliderProjection(normal_source, obj_collider);
          projection_obstacle := OBJ_CalculateColliderProjection(normal_source, obstacle_collider);

          for MakeRange(projection_obj.ranges.count)
          {
            normal := normal_source.normals[it];
            obstacle_dir := obstacle_pos - obj_pos;
            if dot(normal, obstacle_dir) < 0
            continue;

            d := DistanceBetweenRanges(projection_obj.ranges[it], projection_obstacle.ranges[it]);
            if d > 0.0
            {
              // @info(mg) We can exit early from checking this
              //   obstacle since we found an axis that has
              //   a separation between obj and obstacle.
              continue obstacle; // skip this obstacle
            }

            if d > biggest_dist
            {
              biggest_dist = d;
              wall_normal = -normal;
            }
          } // projection loop
        } // sat_iteration loop

        if closest_obstacle_separation_dist > biggest_dist
        {
          closest_obstacle_separation_dist = biggest_dist;
          closest_obstacle_wall_normal = wall_normal;
        }
      } // obstacle loop

      if closest_obstacle_separation_dist < 0.0
      {
        move_out_dir := closest_obstacle_wall_normal;
        move_out_magnitude := -closest_obstacle_separation_dist;
        move_out := move_out_dir * move_out_magnitude;
        obj_pos = obj_pos + move_out;

        // Remove all velocity on collision axis
        // We might want to do something different here in the future!
        if move_out.x obj_dp.x = 0;
        if move_out.y obj_dp.y = 0;
      }
      else
      {
        // Collision not found, stop iterating
        break;
      }
    } // collision_iteration loop

    prev_obj_pos := obj.s.p;
    obj.s.p.x = obj_pos.x;
    obj.s.p.y = obj_pos.y;
    obj.s.moved_dp = obj.s.p - prev_obj_pos;
  } // obj loop

  for * obj: G.obj.all_objects
  {
    if OBJ_HasAllFlags(obj, .ANIMATE_ROTATION)
    {
      lensq := length_squared(obj.s.moved_dp);
      if lensq > square(0.05 * TICK_FLOAT_STEP)
      {
        dir := normalize(obj.s.moved_dp.xy);
        rot := -Atan2(dir) + 0.25;
        rot = WrapFloat(-0.5, 0.5, rot);
        obj.s.rotation = RotationAroundAxis(AxisV3(.Z), rot);
      }
    }
  }
}

TICK_Playback :: ()
{
  smallest_latest_server_tick: u64 = U64_MAX;
  biggest_oldest_server_tick: u64 = 0;
  for snap: G.client.snaps_of_objs
  {
    if snap.latest_server_tick < smallest_latest_server_tick
      smallest_latest_server_tick = snap.latest_server_tick;

    if snap.oldest_server_tick > biggest_oldest_server_tick
      biggest_oldest_server_tick = snap.oldest_server_tick;
  }

  if biggest_oldest_server_tick > G.client.next_playback_tick
  {
    Nlog(LOG_NetTick, #run String.join(
      "Server is too ahead from the client;",
      "biggest_oldest_server_tick: %,",
      "smallest_latest_server_tick: %,",
      "client's next_playback_tick: %,",
      "playback delay: %,",
      "playback catchup %,",
      "[bumping client's next_playback_tick]"),
      biggest_oldest_server_tick,
      smallest_latest_server_tick,
      G.client.next_playback_tick,
      G.client.current_playback_delay,
      G.client.playable_tick_deltas.tick_catchup);

    G.client.next_playback_tick = biggest_oldest_server_tick;
  }

  if smallest_latest_server_tick < G.client.next_playback_tick
  {
    Nlog(LOG_NetTick, #run String.join(
      "Ran out of tick playback state; ",
      "next_playback_tick: %, ",
      "smallest_latest_server_tick: %, ",
      "playback delay: %, ",
      "playback catchup %"),
      G.client.next_playback_tick,
      smallest_latest_server_tick,
      G.client.current_playback_delay,
      G.client.playable_tick_deltas.tick_catchup);
    return;
  }

  // calc current delay
  {
    current_playback_delay_u64 := smallest_latest_server_tick - G.client.next_playback_tick;
    G.client.current_playback_delay = CastSaturate(u16, current_playback_delay_u64);
  }

  if TickDeltas_AddTick(*G.client.playable_tick_deltas, smallest_latest_server_tick)
  {
    TickDeltas_UpdateCatchup(*G.client.playable_tick_deltas, G.client.current_playback_delay);
    if G.client.playable_tick_deltas.tick_catchup
    {
      Nlog(LOG_NetCatchup, "Current playback delay: %d,  Setting playback catchup to %d",
        G.client.current_playback_delay, G.client.playable_tick_deltas.tick_catchup);
    }
  }

  for net_index: MakeRange(OBJ_MAX_NETWORK_OBJECTS.(u32))
  {
    interpolated_sync := CLIENT_LerpObjSync(net_index, G.client.next_playback_tick);
    net_obj := OBJ_FromNetIndex(net_index);
    net_obj.s = interpolated_sync;
  }
  G.client.next_playback_tick += 1;
}
