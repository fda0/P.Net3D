//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static void Game_VerticesCameraTransform(AppState *app, V2 verts[4], float camera_scale, V2 window_transform)
{
    ForU32(i, 4)
    {
        // apply camera transform
        verts[i].x -= app->camera_p.x;
        verts[i].y -= app->camera_p.y;

        verts[i].x *= camera_scale;
        verts[i].y *= camera_scale;

        verts[i].x += window_transform.x;
        verts[i].y += window_transform.y;

        // fix y axis direction to +Y up (SDL uses +Y down, -Y up)
        verts[i].y = app->window_height - verts[i].y;
    }
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
        float camera_scale = 1.f;
        {
            float wh = Max(app->window_width, app->window_height); // pick bigger window dimension
            camera_scale = wh / app->camera_range;
        }
        V2 window_transform = (V2){app->window_width*0.5f, app->window_height*0.5f};

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
            Game_VerticesCameraTransform(app, verts, camera_scale, window_transform);

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

            int indices[] = { 0, 1, 3, 1, 2, 3 };
            SDL_RenderGeometry(app->renderer, sprite->tex,
                               sdl_verts, ArrayCount(sdl_verts),
                               indices, ArrayCount(indices));
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
                Game_VerticesCameraTransform(app, verts, camera_scale, window_transform);

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

                int indices[] = { 0, 1, 3, 1, 2, 3 };
                SDL_RenderGeometry(app->renderer, overlay_sprite->tex,
                                   sdl_verts, ArrayCount(sdl_verts),
                                   indices, ArrayCount(indices));
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
        SDL_SetRenderDrawColorFloat(app->renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(app->renderer, &rect);
    }

    // draw mouse
    {
        static float r = 0;
        r += app->dt * 90.f;
        if (r > 255) r = 0;
        /* set the color to white */
        SDL_SetRenderDrawColor(app->renderer, (int)r, 255 - (int)r, 255, 255);

        float dim = 20;
        SDL_FRect rect = {
            app->mouse.x - 0.5f*dim, app->mouse.y - 0.5f*dim,
            dim, dim
        };
        SDL_RenderFillRect(app->renderer, &rect);
    }
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

    // move camera
    {
        Object *player = Object_Get(app, app->client.player_key, ObjCategory_Net);
        if (!Object_IsNil(player))
        {
            app->camera_p = player->p;
        }
    }

    Game_IssueDrawCommands(app);
}

static Object *Object_CreatePlayer(AppState *app)
{
    Object *player = Object_Create(app, ObjCategory_Net,
                                   app->sprite_dude_id,
                                   ObjectFlag_Draw|ObjectFlag_Move|ObjectFlag_Collide);
    player->sprite_color = ColorF_RGB(1, 0.1f, 0.1f);
    return player;
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

    // add walls
    {
        float thickness = 20.f;
        float length = 400.f;
        float off = length*0.5f - thickness*0.5f;
        Object_Wall(app, (V2){off, 0}, (V2){thickness, length});
        Object_Wall(app, (V2){-off, 0}, (V2){thickness, length});
        Object_Wall(app, (V2){0, off}, (V2){length, thickness});
        Object_Wall(app, (V2){0,-off}, (V2){length*0.5f, thickness});

        if (1) {
            Object *ref = Object_Create(app, ObjCategory_Const,
                                        Sprite_IdFromPointer(app, sprite_ref),
                                        ObjectFlag_Draw|ObjectFlag_Collide);
            ref->p = (V2){0, off*0.5f};
            ref->sprite_color = ColorF_RGBA(1,1,1,1);
        }
        {
            Object *crate = Object_Create(app, ObjCategory_Const,
                                          Sprite_IdFromPointer(app, sprite_crate),
                                          ObjectFlag_Draw|ObjectFlag_Collide);
            crate->p = (V2){0.5f*off, -0.5f*off};
            (void)crate;
        }
    }
}
