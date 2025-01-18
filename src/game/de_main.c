//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static V2 Game_WorldToScreen(AppState *app, V2 pos)
{
    V2 window_transform = (V2){app->window_width*0.5f, app->window_height*0.5f};

    // apply camera transform
    pos.x -= app->camera_p.x;
    pos.y -= app->camera_p.y;

    pos.x *= app->camera_scale;
    pos.y *= app->camera_scale;

    pos.x += window_transform.x;
    pos.y += window_transform.y;

    // fix y axis direction to +Y up (SDL uses +Y down, -Y up)
    pos.y = app->window_height - pos.y;
    return pos;
}

static V2 Game_ScreenToWorld(AppState *app, V2 pos)
{
    V2 window_transform = (V2){app->window_width*0.5f, app->window_height*0.5f};

    float scale_inv = 1.f / app->camera_scale;

    // fix y axis direction to +Y up (SDL uses +Y down, -Y up)
    pos.y = app->window_height - pos.y;

    pos.x -= window_transform.x;
    pos.y -= window_transform.y;

    pos.x *= scale_inv;
    pos.y *= scale_inv;

    // apply camera transform
    pos.x += app->camera_p.x;
    pos.y += app->camera_p.y;
    return pos;
}

static void Game_VerticesCameraTransform(AppState *app, V2 verts[4])
{
    ForU32(i, 4)
        verts[i] = Game_WorldToScreen(app, verts[i]);
}

static void Game_IssueDrawCommands(AppState *app)
{
    // animate collision overlay texture
    if (app->debug.draw_collision_box)
    {
        Sprite *sprite_overlay = Sprite_Get(app, app->sprite_overlay_id);

        float overlay_speed = 12.f;
        app->debug.collision_sprite_animation_t += overlay_speed * app->dt;

        float period = 1.f;
        while (app->debug.collision_sprite_animation_t > period)
        {
            app->debug.collision_sprite_animation_t -= period;
            app->debug.collision_sprite_frame_index += 1;
        }

        app->debug.collision_sprite_frame_index %= sprite_overlay->tex_frames;
    }

    // draw objects
    {
        ForArray(obj_index, app->all_objects)
        {
            Object *obj = app->all_objects + obj_index;
            if (!(obj->flags & ObjectFlag_Draw)) continue;

            Sprite *sprite = Sprite_Get(app, obj->sprite_id);

            V2 verts[4];
            if (sprite->tex)
            {
                V2 tex_half_dim = {(float)sprite->tex->w, (float)sprite->tex->h};
                tex_half_dim.y /= (float)sprite->tex_frames;
                tex_half_dim = V2_Scale(tex_half_dim, 0.5f);

                verts[0] = (V2){-tex_half_dim.x, -tex_half_dim.y};
                verts[1] = (V2){ tex_half_dim.x, -tex_half_dim.y};
                verts[2] = (V2){ tex_half_dim.x,  tex_half_dim.y};
                verts[3] = (V2){-tex_half_dim.x,  tex_half_dim.y};
            }
            else
            {
                static_assert(sizeof(verts) == sizeof(sprite->collision_vertices.arr));
                memcpy(verts, sprite->collision_vertices.arr, sizeof(verts));
            }

            Vertices_Offset(verts, ArrayCount(verts), obj->p);
            Game_VerticesCameraTransform(app, verts);

            SDL_FColor fcolor = ColorF_To_SDL_FColor(obj->sprite_color);
            SDL_Vertex sdl_verts[4];
            SDL_zeroa(sdl_verts);

            static_assert(ArrayCount(verts) == ArrayCount(sdl_verts));
            ForArray(i, sdl_verts)
            {
                sdl_verts[i].position = V2_To_SDL_FPoint(verts[i]);
                sdl_verts[i].color = fcolor;
            }

            {
                float tex_y0 = 0.f;
                float tex_y1 = 1.f;
                if (sprite->tex_frames > 1)
                {
                    Uint32 frame_index = obj->sprite_frame_index;
                    float tex_height = 1.f / sprite->tex_frames;
                    tex_y0 = frame_index * tex_height;
                    tex_y1 = tex_y0 + tex_height;
                }
                sdl_verts[0].tex_coord = (SDL_FPoint){0, tex_y1};
                sdl_verts[1].tex_coord = (SDL_FPoint){1, tex_y1};
                sdl_verts[2].tex_coord = (SDL_FPoint){1, tex_y0};
                sdl_verts[3].tex_coord = (SDL_FPoint){0, tex_y0};
            }

            V2 shader_scale = {0.05f, -0.05f};

            if (obj->flags & ObjectFlag_Move)
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
                    move.x = obj->p.x * shader_scale.x;
                    move.z = obj->p.y * shader_scale.y;
                    Mat4 move_mat = Mat4_Translation(move);
                    Mat4 rot_mat = Mat4_Rotation_RH(-0.06f, (V3){0,0,1});
                    if (obj->p.x == obj->prev_p.x && obj->p.y == obj->prev_p.y)
                        rot_mat = Mat4_Diagonal(1.f);

                    inst_data->transform = Mat4_Mul(move_mat, rot_mat);
                }
            }
            else
            {
                U32 vert_count = 8;
                if (APP.rdr.wall_vert_count + vert_count <= ArrayCount(APP.rdr.wall_verts))
                {
                    V3 move = {};
                    move.x = obj->p.x * shader_scale.x;
                    move.z = obj->p.y * shader_scale.y;

                    Rdr_Vertex *vert_start = APP.rdr.wall_verts + APP.rdr.wall_vert_count;
                    APP.rdr.wall_vert_count += vert_count;

                    ForU32(i, vert_count)
                    {
                        Rdr_Vertex *vrt = vert_start + i;
                        SDL_zerop(vrt);
                        vrt->color = (V3){0.5f, 0.12f, 0};
                        vrt->normal = (V3){1,1,0};
                        if (i > 3)
                        {
                            vrt->color = (V3){0.67f, 0.45f, 0.05f};
                        }
                    }

                    Col_Vertices col = sprite->collision_vertices;
                    vert_start[0].p = V3_Make_XZ_Y(col.arr[0], 0.f);
                    vert_start[1].p = V3_Make_XZ_Y(col.arr[1], 0.f);
                    vert_start[2].p = V3_Make_XZ_Y(col.arr[2], 0.f);
                    vert_start[3].p = V3_Make_XZ_Y(col.arr[3], 0.f);
                    vert_start[4].p = V3_Make_XZ_Y(col.arr[0], 40.f);
                    vert_start[5].p = V3_Make_XZ_Y(col.arr[1], 40.f);
                    vert_start[6].p = V3_Make_XZ_Y(col.arr[2], 40.f);
                    vert_start[7].p = V3_Make_XZ_Y(col.arr[3], 40.f);

                    ForU32(i, vert_count)
                    {
                        Rdr_Vertex *vrt = vert_start + i;
                        vrt->p.x += obj->p.x;
                        vrt->p.z += obj->p.y;
                        vrt->p.x *= shader_scale.x;
                        vrt->p.y *= shader_scale.x;
                        vrt->p.z *= shader_scale.y;
                    }
                }
            }
        }

        if (app->debug.draw_collision_box)
        {
            ForArray(obj_index, app->all_objects)
            {
                Object *obj = app->all_objects + obj_index;
                if (!Object_HasAllFlags(obj, ObjectFlag_Collide|ObjectFlag_Draw)) continue;

                Sprite *sprite = Sprite_Get(app, obj->sprite_id);

                V2 verts[4];
                static_assert(sizeof(verts) == sizeof(sprite->collision_vertices.arr));
                memcpy(verts, sprite->collision_vertices.arr, sizeof(verts));
                Vertices_Offset(verts, ArrayCount(verts), obj->p);
                Game_VerticesCameraTransform(app, verts);

                ColorF color = ColorF_RGBA(1, 0, 0.8f, 0.8f);
                if (obj->has_collision)
                {
                    color.g = 1;
                    color.a = 1;
                }

                SDL_FColor fcolor = ColorF_To_SDL_FColor(color);
                SDL_Vertex sdl_verts[4];
                SDL_zeroa(sdl_verts);

                static_assert(ArrayCount(verts) == ArrayCount(sdl_verts));
                ForArray(i, sdl_verts)
                {
                    sdl_verts[i].position = V2_To_SDL_FPoint(verts[i]);
                    sdl_verts[i].color = fcolor;
                }

                Sprite *overlay_sprite = Sprite_Get(app, app->sprite_overlay_id);
                {
                    float tex_y0 = 0.f;
                    float tex_y1 = 1.f;
                    if (overlay_sprite->tex_frames > 1)
                    {
                        Uint32 frame_index = app->debug.collision_sprite_frame_index;
                        float tex_height = 1.f / overlay_sprite->tex_frames;
                        tex_y0 = frame_index * tex_height;
                        tex_y1 = tex_y0 + tex_height;
                    }
                    sdl_verts[0].tex_coord = (SDL_FPoint){0, tex_y1};
                    sdl_verts[1].tex_coord = (SDL_FPoint){1, tex_y1};
                    sdl_verts[2].tex_coord = (SDL_FPoint){1, tex_y0};
                    sdl_verts[3].tex_coord = (SDL_FPoint){0, tex_y0};
                }

#if 0
                int indices[] = { 0, 1, 3, 1, 2, 3 };
                SDL_RenderGeometry(app->renderer, overlay_sprite->tex,
                                   sdl_verts, ArrayCount(sdl_verts),
                                   indices, ArrayCount(indices));
#endif
            }
        }
    }

    // draw debug networking stuff
    {
        ColorF green = ColorF_RGB(0, 1, 0);
        ColorF red = ColorF_RGB(1, 0, 0);

        SDL_FRect rect = { 0, 0, 30, 30 };
        if (!app->net.is_server) rect.x = 40;

        ColorF color = app->net.err ? red : green;
#if 0
        SDL_SetRenderDrawColorFloat(app->renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(app->renderer, &rect);
#else
        (void)color;
#endif
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
    {
        // pos
        Object *player = Object_Get(app, app->client.player_key, ObjCategory_Net);
        if (!Object_IsNil(player))
        {
            app->camera_p = player->p;
        }

        // scale
        {
            float wh = Max(app->window_width, app->window_height); // pick bigger window dimension
            app->camera_scale = wh / app->camera_range;
        }

        app->world_mouse = Game_ScreenToWorld(app, app->mouse);
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
        app->debug.draw_collision_box = true;
        app->log_filter &= ~(LogFlags_NetDatagram);
    }

    Net_Init(app);

    app->frame_time = SDL_GetTicks();
    app->sprite_count += 1; // reserve sprite under index 0 as special 'nil' value
    app->camera_range = 500;
    app->camera_scale = 1.f;
    app->tick_id = Max(NET_MAX_TICK_HISTORY, NET_CLIENT_MAX_SNAPSHOTS);

    // this assumes we run the game from build directory
    // @todo in the future we should force CWD or query demongus absolute path etc
    Sprite *sprite_overlay = Sprite_Create(app, "../res/pxart/overlay.png", 6);
    app->sprite_overlay_id = Sprite_IdFromPointer(app, sprite_overlay);

    Sprite *sprite_crate = Sprite_Create(app, "../res/pxart/crate.png", 1);
    {
        Sprite_CollisionVerticesRotate(sprite_crate, 0.125f);
        Sprite_CollisionVerticesScale(sprite_crate, 0.6f);
        Sprite_CollisionVerticesOffset(sprite_crate, (V2){0, -3});
        Sprite_RecalculateCollsionNormals(sprite_crate);
    }

    Sprite *sprite_dude = Sprite_Create(app, "../res/pxart/dude_walk.png", 5);
    {
        sprite_dude->collision_vertices = Vertices_FromRect((V2){0}, (V2){20, 10});
        Sprite_CollisionVerticesOffset(sprite_dude, (V2){0, -8});
        app->sprite_dude_id = Sprite_IdFromPointer(app, sprite_dude);
    }
    Sprite *sprite_ref = Sprite_Create(app, "../res/pxart/reference.png", 1);
    Sprite *sprite_cross = Sprite_Create(app, "../res/pxart/cross.png", 1);


    // add walls
    {
        float thickness = 20.f;
        float length = 400.f;
        float off = length*0.5f - thickness*0.5f;
        Object_CreateWall(app, (V2){off, 0}, (V2){thickness, length});
        Object_CreateWall(app, (V2){-off, 0}, (V2){thickness, length});
        Object_CreateWall(app, (V2){0, off}, (V2){length, thickness});
        Object_CreateWall(app, (V2){0,-off}, (V2){length*0.5f, thickness});

        if (1)
        {
            Object *ref = Object_Create(app, ObjCategory_Local,
                                        Sprite_IdFromPointer(app, sprite_ref),
                                        ObjectFlag_Draw|ObjectFlag_Collide);
            ref->p = (V2){0, off*0.5f};
            ref->sprite_color = ColorF_RGBA(1,1,1,1);
        }
        {
            Object *crate = Object_Create(app, ObjCategory_Local,
                                          Sprite_IdFromPointer(app, sprite_crate),
                                          ObjectFlag_Draw|ObjectFlag_Collide);
            crate->p = (V2){0.5f*off, -0.5f*off};
            (void)crate;
        }
    }

    // pathing marker
    {
        app->pathing_marker = Object_Create(app, ObjCategory_Local, Sprite_IdFromPointer(app, sprite_cross), 0)->key;
    }
}
