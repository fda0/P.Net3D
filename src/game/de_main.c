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

        if (obj->flags & ObjectFlag_ModelTeapot ||
            obj->flags & ObjectFlag_ModelFlag)
        {
            bool model_enabled[RdrModel_COUNT] =
            {
                obj->flags & ObjectFlag_ModelTeapot,
                obj->flags & ObjectFlag_ModelFlag
            };

            static_assert(ArrayCount(model_enabled) == ArrayCount(APP.rdr.models));
            ForArray(i, APP.rdr.models)
            {
                if (model_enabled[i] &&
                    APP.rdr.models[i].count < ArrayCount(APP.rdr.models[i].data))
                {
                    Rdr_ModelInstanceData *inst = APP.rdr.models[i].data + APP.rdr.models[i].count;
                    APP.rdr.models[i].count += 1;

                    SDL_zerop(inst);
                    inst->color.x = obj->sprite_color.r;
                    inst->color.y = obj->sprite_color.g;
                    inst->color.z = obj->sprite_color.b;

                    V3 move = {};
                    move.x = obj->p.x;
                    move.y = obj->p.y;
                    Mat4 move_mat = Mat4_Translation(move);
                    Mat4 rot_mat = Mat4_Rotation_RH(0.03f, (V3){0,1,0});
                    if (obj->p.x == obj->prev_p.x && obj->p.y == obj->prev_p.y)
                        rot_mat = Mat4_Diagonal(1.f);

                    inst->transform = Mat4_Mul(move_mat, rot_mat);

                }
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
                    SDL_zerop(&wall_verts[i]);
                    wall_verts[i].color = (V3){0.95f, 0.7f, 0};
                }

                float height = 40.f;
                CollisionVertices collision = obj->collision.verts;
                {
                    U32 cube_index_map[] =
                    {
                        4,5,0,5,1,0, // E
                        7,6,3,6,2,3, // W
                        4,7,0,7,3,0, // N
                        6,5,2,5,1,2, // S
                        4,5,7,5,6,7, // Top
                        3,2,0,2,1,0, // Bottom
                    };
                    Assert(ArrayCount(cube_index_map) == vert_count);

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

                    ForArray(i, cube_index_map)
                    {
                        U32 index = cube_index_map[i];
                        wall_verts[i].p = cube_verts[index];
                        wall_verts[i].p.x += obj->p.x;
                        wall_verts[i].p.y += obj->p.y;
                    }
                }

                float w0 = V2_Length(V2_Sub(collision.arr[0], collision.arr[1]));
                float w1 = V2_Length(V2_Sub(collision.arr[1], collision.arr[2]));
                float w2 = V2_Length(V2_Sub(collision.arr[2], collision.arr[3]));
                float w3 = V2_Length(V2_Sub(collision.arr[3], collision.arr[0]));
                float texels_per_cm = 0.05f;

                ForU32(face_i, face_count)
                {
                    // face texture UVs
                    V2 face_dim = {};
                    switch (face_i)
                    {
                        case 0: /* E */ face_dim = (V2){w0, height}; break;
                        case 1: /* W */ face_dim = (V2){w2, height}; break;
                        case 2: /* N */ face_dim = (V2){w3, height}; break;
                        case 3: /* S */ face_dim = (V2){w1, height}; break;
                        case 4: /* T */ face_dim = (V2){w0, w1}; break; // works for rects only
                        case 5: /* B */ face_dim = (V2){w0, w1}; break; // works for rects only
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

                    // face normals
                    V3 face_normal = {};
                    switch (face_i)
                    {
                        case 0: /* E */ face_normal = V3_Make_XY_Z(obj->collision.norms.arr[0], 0); break;
                        case 1: /* W */ face_normal = V3_Make_XY_Z(obj->collision.norms.arr[2], 0); break;
                        case 2: /* N */ face_normal = V3_Make_XY_Z(obj->collision.norms.arr[3], 0); break;
                        case 3: /* S */ face_normal = V3_Make_XY_Z(obj->collision.norms.arr[1], 0); break;
                        case 4: /* T */ face_normal = (V3){0,0,1}; break;
                        case 5: /* B */ face_normal = (V3){0,0,-1}; break;
                    }

                    ForU32(vert_i, vertices_per_face)
                    {
                        U32 i = (face_i * vertices_per_face) + vert_i;
                        wall_verts[i].uv = V3_Make_XY_Z(face_uvs[vert_i], (float)face_i);
                        wall_verts[i].normal = face_normal;
                    }
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
        app->pathing_marker = Object_Create(app, ObjCategory_Local, ObjectFlag_ModelFlag)->key;
    }
}
