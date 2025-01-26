//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static void Game_IssueDrawCommands(AppState *app)
{
    // draw objects
    ForArray(obj_index, app->all_objects)
    {
        Object *obj = app->all_objects + obj_index;
        if (!(obj->flags & ObjectFlag_Draw)) continue;

        if (obj->flags & ObjectFlag_RenderTeapot)
        {
            if (APP.rdr.instance_count < ArrayCount(APP.rdr.instance_data))
            {
                U32 instance_index = APP.rdr.instance_count;
                APP.rdr.instance_count += 1;
                Rdr_ModelInstanceData *inst_data = APP.rdr.instance_data + instance_index;
                SDL_zerop(inst_data);

                inst_data->color.x = obj->sprite_color.r;
                inst_data->color.y = obj->sprite_color.g;
                inst_data->color.z = obj->sprite_color.b;

                V3 move = {};
                move.x = obj->p.x;
                move.y = obj->p.y;
                Mat4 move_mat = Mat4_Translation(move);
                Mat4 rot_mat = Mat4_Rotation_RH(0.03f, (V3){0,1,0});
                if (obj->p.x == obj->prev_p.x && obj->p.y == obj->prev_p.y)
                    rot_mat = Mat4_Diagonal(1.f);

                inst_data->transform = Mat4_Mul(move_mat, rot_mat);
            }
        }
        else
        {
            U32 face_count = 6;
            U32 vertices_per_face = 3*2;
            U32 vert_count = face_count * vertices_per_face;

            if (APP.rdr.wall_vert_count + vert_count <= ArrayCount(APP.rdr.wall_verts))
            {
                Rdr_WallVertex *wall_verts = APP.rdr.wall_verts + APP.rdr.wall_vert_count;
                APP.rdr.wall_vert_count += vert_count;

                ForU32(i, vert_count)
                {
                    Rdr_WallVertex *vrt = wall_verts + i;
                    SDL_zerop(vrt);
                    vrt->color = (V3){0.95f, 0.7f, 0};
                    vrt->normal = (V3){1,1,0};
                }

                float height = 30.f;
                CollisionVertices collision = obj->collision.verts;
                {
                    V3 cube_verts[8] =
                    {
                        V3_Make_XY_Z(collision.arr[0], 0.f),
                        V3_Make_XY_Z(collision.arr[1], 0.f),
                        V3_Make_XY_Z(collision.arr[2], 0.f),
                        V3_Make_XY_Z(collision.arr[3], 0.f),
                        V3_Make_XY_Z(collision.arr[0], height),
                        V3_Make_XY_Z(collision.arr[1], height),
                        V3_Make_XY_Z(collision.arr[2], height),
                        V3_Make_XY_Z(collision.arr[3], height),
                    };

                    U32 i = 0;
                    // front
                    wall_verts[i++].p = cube_verts[0];
                    wall_verts[i++].p = cube_verts[1];
                    wall_verts[i++].p = cube_verts[4];
                    wall_verts[i++].p = cube_verts[1];
                    wall_verts[i++].p = cube_verts[5];
                    wall_verts[i++].p = cube_verts[4];
                    // bottom
                    wall_verts[i++].p = cube_verts[3];
                    wall_verts[i++].p = cube_verts[2];
                    wall_verts[i++].p = cube_verts[0];
                    wall_verts[i++].p = cube_verts[2];
                    wall_verts[i++].p = cube_verts[1];
                    wall_verts[i++].p = cube_verts[0];
                    // back
                    wall_verts[i++].p = cube_verts[7];
                    wall_verts[i++].p = cube_verts[6];
                    wall_verts[i++].p = cube_verts[3];
                    wall_verts[i++].p = cube_verts[6];
                    wall_verts[i++].p = cube_verts[2];
                    wall_verts[i++].p = cube_verts[3];
                    // top
                    wall_verts[i++].p = cube_verts[4];
                    wall_verts[i++].p = cube_verts[5];
                    wall_verts[i++].p = cube_verts[7];
                    wall_verts[i++].p = cube_verts[5];
                    wall_verts[i++].p = cube_verts[6];
                    wall_verts[i++].p = cube_verts[7];
                    // left
                    wall_verts[i++].p = cube_verts[4];
                    wall_verts[i++].p = cube_verts[7];
                    wall_verts[i++].p = cube_verts[0];
                    wall_verts[i++].p = cube_verts[7];
                    wall_verts[i++].p = cube_verts[3];
                    wall_verts[i++].p = cube_verts[0];
                    // right
                    wall_verts[i++].p = cube_verts[6];
                    wall_verts[i++].p = cube_verts[5];
                    wall_verts[i++].p = cube_verts[2];
                    wall_verts[i++].p = cube_verts[5];
                    wall_verts[i++].p = cube_verts[1];
                    wall_verts[i++].p = cube_verts[2];
                    Assert(i == vert_count);
                }

                float w0 = V2_Length(V2_Sub(collision.arr[0], collision.arr[1]));
                float w1 = V2_Length(V2_Sub(collision.arr[1], collision.arr[2]));
                float w2 = V2_Length(V2_Sub(collision.arr[2], collision.arr[3]));
                float w3 = V2_Length(V2_Sub(collision.arr[3], collision.arr[0]));
                float texels_per_cm = 0.05f;

                ForU32(face_i, face_count)
                {
                    V2 face_dim = {1, 1};
                    switch (face_i)
                    {
                        case 0: /* front */  face_dim = (V2){w0, height}; break;
                        case 1: /* bottom */ face_dim = (V2){w0, w1}; break; // works for rects only
                        case 2: /* back */   face_dim = (V2){w2, height}; break;
                        case 3: /* top */    face_dim = (V2){w0, w1}; break; // works for rects only
                        case 4: /* left */   face_dim = (V2){w3, height}; break;
                        case 5: /* right */  face_dim = (V2){w1, height}; break;
                    }

                    face_dim = V2_Scale(face_dim, texels_per_cm);
                    V2 face_uvs[6] =
                    {
                        (V2){0, 0},
                        (V2){face_dim.x, 0},
                        (V2){0, face_dim.y},
                        (V2){face_dim.x, 0},
                        (V2){face_dim.x, face_dim.y},
                        (V2){0, face_dim.y},
                    };

                    ForU32(vert_i, vertices_per_face)
                    {
                        Rdr_WallVertex *vrt = wall_verts + (vert_i + face_i * vertices_per_face);
                        AssertBounds(vert_i, face_uvs);
                        vrt->uv = face_uvs[vert_i];
                    }
                }

                ForU32(i, vert_count)
                {
                    Rdr_WallVertex *vrt = wall_verts + i;
                    vrt->p.x += obj->p.x;
                    vrt->p.y += obj->p.y;
                }
            }
        }
    }
}

static void Game_SetWindowPosSize(AppState *app, Sint32 px, Sint32 py, Sint32 w, Sint32 h)
{
    SDL_SetWindowPosition(app->window, px, py);
    SDL_SetWindowSize(app->window, w, h);
}

static void Game_AutoLayoutApply(AppState *app, Uint32 user_count,
                                 Sint32 px, Sint32 py, Sint32 w, Sint32 h)
{
    if (user_count == app->latest_autolayout_user_count)
        return;

    app->latest_autolayout_user_count = user_count;
    Game_SetWindowPosSize(app, px, py, w, h);
}


static void Game_Iterate(AppState *app)
{
    Test_Iterate();

    {
        app->frame_id += 1;

        Uint64 new_frame_time = SDL_GetTicks();
        Uint64 delta_time = new_frame_time - app->frame_time;
        app->frame_time = new_frame_time;
        app->dt = delta_time * (0.001f);
        app->tick_dt_accumulator += Min(app->dt, 1.f); // clamp dt to 1s

        if (app->debug.fixed_dt)
        {
            app->dt = app->debug.fixed_dt;
        }

        app->at = WrapF(0.f, 1000.f, app->at + app->dt);
    }

    Net_IterateReceive(app);

    if (app->debug.single_tick_stepping)
    {
        if (app->debug.unpause_one_tick)
        {
            app->tick_id += 1;
            Tick_Iterate(app);
            app->debug.unpause_one_tick = false;
        }
    }
    else
    {
        while (app->tick_dt_accumulator > TIME_STEP)
        {
            app->tick_id += 1;
            app->tick_dt_accumulator -= TIME_STEP;
            Tick_Iterate(app);
        }
    }

    Net_IterateSend(app);


    // move camera; calculate camera scale
    if (APP.debug.noclip_camera)
    {
        V3 camera_dir = {0};
        if (app->keyboard[SDL_SCANCODE_I]) camera_dir.x += 1;
        if (app->keyboard[SDL_SCANCODE_K]) camera_dir.x -= 1;
        if (app->keyboard[SDL_SCANCODE_J]) camera_dir.y += 1;
        if (app->keyboard[SDL_SCANCODE_L]) camera_dir.y -= 1;
        if (app->keyboard[SDL_SCANCODE_O]) camera_dir.z += 1;
        if (app->keyboard[SDL_SCANCODE_U]) camera_dir.z -= 1;
        camera_dir = V3_Normalize(camera_dir);
        camera_dir = V3_Scale(camera_dir, APP.dt * 10.f);
        APP.camera_p = V3_Add(APP.camera_p, camera_dir);
    }
    else
    {
        Object *player = Object_Get(app, app->client.player_key, ObjCategory_Net);
        if (!Object_IsNil(player))
        {
            app->camera_p = V3_Make_XY_Z(player->p, 70.f);
            app->camera_p.x -= 50.f;
        }
    }

    if (app->mouse_keys & SDL_BUTTON_RMASK)
    {
        Object *marker = Object_Get(app, app->pathing_marker, ObjCategory_Local);
        if (!Object_IsNil(marker))
        {
            marker->flags |= ObjectFlag_Draw;
            marker->p = app->world_mouse;
            app->pathing_marker_set = true;
        }
    }

    Game_IssueDrawCommands(app);
}

static void Game_Init(AppState *app)
{
    // init debug options
    {
        //app->debug.fixed_dt = 0.1f;
        //app->debug.single_tick_stepping = true;
        //app->debug.noclip_camera = true;
        app->debug.draw_collision_box = true;
        app->log_filter &= ~(LogFlags_NetDatagram);
    }

    Net_Init(app);

    app->frame_time = SDL_GetTicks();
    app->camera_p = (V3){-50.f, 0, 70.f};
    app->camera_rot = (V3){0, -0.1f, 0};
    app->tick_id = Max(NET_MAX_TICK_HISTORY, NET_CLIENT_MAX_SNAPSHOTS);

    // add walls
    {
        float thickness = 20.f;
        float length = 400.f;
        float off = length*0.5f - thickness*0.5f;
        Object_CreateWall(app, (V2){off, 0}, (V2){thickness, length});
        Object_CreateWall(app, (V2){-off, 0}, (V2){thickness, length});
        Object_CreateWall(app, (V2){0, off}, (V2){length, thickness});
        Object_CreateWall(app, (V2){0,-off}, (V2){length*0.5f, thickness});

        {
            Object *rotated_wall = Object_CreateWall(app, (V2){0.5f*off, -0.5f*off},
                                                     (V2){thickness, 2.f*thickness});

            Vertices_Rotate(rotated_wall->collision.verts.arr,
                            ArrayCount(rotated_wall->collision.verts.arr),
                            0.1f);

            Collision_RecalculateNormals(&rotated_wall->collision);
        }
    }

    // pathing marker
    {
        app->pathing_marker = Object_Create(app, ObjCategory_Local, 0)->key;
    }
}
