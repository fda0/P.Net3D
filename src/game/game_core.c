//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static void Game_AnimateObjects()
{
    ForArray(obj_index, APP.all_objects)
    {
        Object *obj = APP.all_objects + obj_index;

        if (Obj_HasAnyFlag(obj, ObjectFlag_ModelTeapot))
        {
            int z = 1;
            z += 1;
            z += 1;
            z += 1;
        }

        if (Obj_HasAllFlags(obj, ObjectFlag_AnimateRotation))
        {
            //V2 obj_dir = obj->dp;
            V2 obj_dir = V2_Sub(obj->s.p, obj->s.prev_p);
            if (obj_dir.x || obj_dir.y)
            {
                obj_dir = V2_Normalize(obj_dir);

                float rot = -Atan2F(obj_dir) + 0.25f;
                obj->s.rot_z = WrapF(-0.5f, 0.5f, rot);
                //LOG(LogFlags_Debug, "rot_z: %f", obj->rot_z);
                int a = 0;
                a += 1;
            }
        }

        if (Obj_HasAnyFlag(obj, ObjectFlag_AnimateRotation))
        {
            Quat q0 = obj->l.animated_rot;
            Quat q1 = Quat_FromAxisAngle_RH((V3){0,0,1}, obj->s.rot_z);

            float w1 = APP.dt * 25.f;
            float w0 = 1.f - w1;

            if (Quat_Inner(q0, q1) < 0.f)
                w1 = -w1;

            obj->l.animated_rot = Quat_Normalize(Quat_Mix(q0, q1, w0, w1));
        }

        if (Obj_HasAnyFlag(obj, ObjectFlag_AnimatePosition))
        {
            V3 pos = V3_Make_XY_Z(obj->s.p, 0);
            V3 delta = V3_Sub(pos, obj->l.animated_p);

            float speed = APP.dt * 15.f;
            delta = V3_Scale(delta, speed);

            obj->l.animated_p = V3_Add(obj->l.animated_p, delta);
            ForArray(i, obj->l.animated_p.E)
            {
                if (obj->l.animated_p.E[i] < 0.1f)
                    obj->l.animated_p.E[i] = pos.E[i];
            }
        }
    }
}

static void Game_DrawObjects()
{
    ForArray(obj_index, APP.all_objects)
    {
        Object *obj = APP.all_objects + obj_index;
        if (!Obj_HasAnyFlag(obj, ObjectFlag_Draw)) continue;

        if (Obj_HasAnyFlag(obj, ObjectFlag_ModelTeapot|ObjectFlag_ModelFlag))
        {
            bool model_enabled[RdrModel_COUNT] =
            {
                obj->s.flags & ObjectFlag_ModelTeapot,
                obj->s.flags & ObjectFlag_ModelFlag
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
                    inst->color.x = obj->s.color.r;
                    inst->color.y = obj->s.color.g;
                    inst->color.z = obj->s.color.b;

                    V3 pos = V3_Make_XY_Z(obj->s.p, 0);
                    if (Obj_HasAnyFlag(obj, ObjectFlag_AnimatePosition))
                    {
                        pos = obj->l.animated_p;
                    }
                    Mat4 move_mat = Mat4_Translation(pos);

                    Mat4 rot_mat = Mat4_Identity();
                    if (Obj_HasAnyFlag(obj, ObjectFlag_AnimateRotation))
                    {
                        rot_mat = Mat4_Rotation_Quat(obj->l.animated_rot);
                        //rot_mat = Mat4_Rotation_RH((V3){0,0,1}, obj->animated_rot_z);
                    }
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

                float height = 20.f;
                float bot_z = 0;
                float top_z = bot_z + height;

                CollisionVertices collision = obj->s.collision.verts;
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
                        V3_Make_XY_Z(collision.arr[0], bot_z),
                        V3_Make_XY_Z(collision.arr[1], bot_z),
                        V3_Make_XY_Z(collision.arr[2], bot_z),
                        V3_Make_XY_Z(collision.arr[3], bot_z),
                        V3_Make_XY_Z(collision.arr[0], top_z),
                        V3_Make_XY_Z(collision.arr[1], top_z),
                        V3_Make_XY_Z(collision.arr[2], top_z),
                        V3_Make_XY_Z(collision.arr[3], top_z),
                    };

                    ForArray(i, cube_index_map)
                    {
                        U32 index = cube_index_map[i];
                        wall_verts[i].p = cube_verts[index];
                        wall_verts[i].p.x += obj->s.p.x;
                        wall_verts[i].p.y += obj->s.p.y;
                    }

                    {
                        V4 test = {};
                        test.x = wall_verts[0].p.x;
                        test.y = wall_verts[0].p.y;
                        test.z = wall_verts[0].p.z;
                        test.w = 1.f;

                        V4 post_view = V4_MulM4(APP.camera_all_mat, test);

                        V4 div = post_view;
                        div.x /= div.w;
                        div.y /= div.w;
                        div.z /= div.w;
                        div.w /= div.w;

                        int a = 0;
                        a += 123;
                        a += 123;
                        a += 123;
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
                        case 0: /* E */ face_normal = V3_Make_XY_Z(obj->s.collision.norms.arr[0], 0); break;
                        case 1: /* W */ face_normal = V3_Make_XY_Z(obj->s.collision.norms.arr[2], 0); break;
                        case 2: /* N */ face_normal = V3_Make_XY_Z(obj->s.collision.norms.arr[3], 0); break;
                        case 3: /* S */ face_normal = V3_Make_XY_Z(obj->s.collision.norms.arr[1], 0); break;
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

static void Game_SetWindowPosSize(I32 px, I32 py, I32 w, I32 h)
{
    SDL_SetWindowPosition(APP.window, px, py);
    SDL_SetWindowSize(APP.window, w, h);
}

static void Game_AutoLayoutApply(U32 user_count,
                                 I32 px, I32 py, I32 w, I32 h)
{
    if (user_count == APP.latest_autolayout_user_count)
        return;

    APP.latest_autolayout_user_count = user_count;
    Game_SetWindowPosSize(px, py, w, h);
}

static void Game_Iterate()
{
    Test_Iterate();

    {
        APP.frame_id += 1;

        U64 new_frame_time = SDL_GetTicks();
        U64 delta_time = new_frame_time - APP.frame_time;
        APP.frame_time = new_frame_time;
        APP.dt = delta_time * (0.001f);
        APP.tick_dt_accumulator += Min(APP.dt, 1.f); // clamp dt to 1s

        if (APP.debug.fixed_dt)
        {
            APP.dt = APP.debug.fixed_dt;
        }

        APP.at = WrapF(0.f, 1000.f, APP.at + APP.dt);
    }

    Net_IterateReceive();

    if (APP.debug.single_tick_stepping)
    {
        if (APP.debug.unpause_one_tick)
        {
            APP.tick_id += 1;
            Tick_Iterate();
            APP.debug.unpause_one_tick = false;
        }
    }
    else
    {
        while (APP.tick_dt_accumulator > TIME_STEP)
        {
            APP.tick_id += 1;
            APP.tick_dt_accumulator -= TIME_STEP;
            Tick_Iterate();
        }
    }

    Net_IterateSend();


    // move camera; calculate camera scale
    if (APP.debug.noclip_camera)
    {
        V3 camera_dir = {0};
        if (APP.keyboard[SDL_SCANCODE_I]) camera_dir.x += 1;
        if (APP.keyboard[SDL_SCANCODE_K]) camera_dir.x -= 1;
        if (APP.keyboard[SDL_SCANCODE_J]) camera_dir.y += 1;
        if (APP.keyboard[SDL_SCANCODE_L]) camera_dir.y -= 1;
        if (APP.keyboard[SDL_SCANCODE_O]) camera_dir.z += 1;
        if (APP.keyboard[SDL_SCANCODE_U]) camera_dir.z -= 1;
        camera_dir = V3_Normalize(camera_dir);
        camera_dir = V3_Scale(camera_dir, APP.dt * 100.f);
        APP.camera_p = V3_Add(APP.camera_p, camera_dir);

        float rot_speed = 0.1f * APP.dt * (APP.keyboard[SDL_SCANCODE_V] ? -1.f : 1.f);
        if (APP.keyboard[SDL_SCANCODE_B]) APP.camera_rot.x += rot_speed;
        if (APP.keyboard[SDL_SCANCODE_N]) APP.camera_rot.y += rot_speed;
        if (APP.keyboard[SDL_SCANCODE_M]) APP.camera_rot.z += rot_speed;
    }
    else
    {
        Object *player = Obj_Get(APP.client.player_key, ObjCategory_Net);
        if (!Obj_IsNil(player))
        {
            APP.camera_p = V3_Make_XY_Z(player->s.p, 70.f);
            APP.camera_p.x -= 50.f;
        }
    }

    // calculate camera transform matrices
    {
        APP.camera_move_mat = Mat4_InvTranslation(Mat4_Translation(APP.camera_p));

        APP.camera_rot_mat = Mat4_Rotation_RH((V3){1,0,0}, APP.camera_rot.x);
        APP.camera_rot_mat = Mat4_Mul(Mat4_Rotation_RH((V3){0,0,1}, APP.camera_rot.z), APP.camera_rot_mat);
        APP.camera_rot_mat = Mat4_Mul(Mat4_Rotation_RH((V3){0,1,0}, APP.camera_rot.y), APP.camera_rot_mat);

        APP.camera_perspective_mat = Mat4_Perspective_RH_NO(APP.camera_fov_y,
                                                            (float)APP.window_width/APP.window_height,
                                                            1.f, 1000.f);

        APP.camera_all_mat = Mat4_Mul(APP.camera_perspective_mat, Mat4_Mul(APP.camera_rot_mat, APP.camera_move_mat));
    }

    // calculate mouse in screen space -> mouse in world space (at Z == 0)
    {
        V2 view_mouse =
        {
            (APP.mouse.x / (float)APP.window_width)  * 2.f - 1.f,
            (APP.mouse.y / (float)APP.window_height) * 2.f - 1.f,
        };

        V3 plane_origin = {};
        V3 plane_normal = {0,0,1};

        float aspect = (float)APP.window_width / APP.window_height;
        float tan = TanF(APP.camera_fov_y * 0.5f);
        aspect *= aspect;
        tan    *= tan;

        float x = tan * view_mouse.x * aspect;
        float y = tan * view_mouse.y;
        V3 aim_dir =
        {
            x * APP.camera_all_mat.elem[0][0] + y * APP.camera_all_mat.elem[0][1] + APP.camera_all_mat.elem[0][2],
            x * APP.camera_all_mat.elem[1][0] + y * APP.camera_all_mat.elem[1][1] + APP.camera_all_mat.elem[1][2],
            x * APP.camera_all_mat.elem[2][0] + y * APP.camera_all_mat.elem[2][1] + APP.camera_all_mat.elem[2][2],
        };

        float t_numerator = V3_Inner(V3_Sub(plane_origin, APP.camera_p), plane_normal);
        float t_denominator = V3_Inner(aim_dir, plane_normal);

        APP.world_mouse_valid = t_denominator < 0.f;
        if (APP.world_mouse_valid)
        {
            float t = t_numerator / t_denominator;
            V3 world_off = V3_Scale(aim_dir, t);
            V3 world_mouse = V3_Add(APP.camera_p, world_off);
            APP.world_mouse = (V2){world_mouse.x, world_mouse.y};
        }
    }

    Game_AnimateObjects();

    if (APP.mouse_keys & SDL_BUTTON_RMASK)
    {
        if (APP.world_mouse_valid)
        {
            Object *marker = Obj_Get(APP.pathing_marker, ObjCategory_Local);
            if (!Obj_IsNil(marker))
            {
                marker->s.flags |= ObjectFlag_Draw;
                marker->s.p = APP.world_mouse;
                marker->l.animated_p = V3_Make_XY_Z(marker->s.p, 30.f);
                APP.pathing_marker_set = true;
            }
        }
    }

    Game_DrawObjects();
}

static void Game_Init()
{
    // init debug options
    {
        //APP.debug.fixed_dt = 0.1f;
        //APP.debug.single_tick_stepping = true;
        APP.debug.noclip_camera = true;
        APP.debug.draw_collision_box = true;
        APP.log_filter &= ~(LogFlags_NetDatagram);
    }

    Net_Init();

    APP.frame_time = SDL_GetTicks();
    APP.camera_fov_y = 0.19f;
    APP.camera_p = (V3){-200.f, 0.f, 175.f};
    APP.camera_rot = (V3){0, -0.15f, 0};
    APP.tick_id = Max(NET_MAX_TICK_HISTORY, NET_CLIENT_MAX_SNAPSHOTS);

    // add walls
    {
        float thickness = 20.f;
        float length = 400.f;
        float off = length*0.5f - thickness*0.5f;
        Obj_CreateWall((V2){off, 0}, (V2){thickness, length});
        Obj_CreateWall((V2){-off, 0}, (V2){thickness, length});
        Obj_CreateWall((V2){0, off}, (V2){length, thickness});
        Obj_CreateWall((V2){0,-off}, (V2){length*0.5f, thickness});

        {
            Object *rotated_wall = Obj_CreateWall((V2){0.5f*off, -0.5f*off},
                                                     (V2){thickness, 2.f*thickness});

            Vertices_Rotate(rotated_wall->s.collision.verts.arr,
                            ArrayCount(rotated_wall->s.collision.verts.arr),
                            0.1f);

            Collision_RecalculateNormals(&rotated_wall->s.collision);
        }
    }

    // pathing marker
    {
        APP.pathing_marker = Obj_Create(ObjCategory_Local, ObjectFlag_ModelFlag |
                                        ObjectFlag_AnimatePosition)->s.key;
    }
}
