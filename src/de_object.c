static Object *Object_Get(AppState *app, Uint32 id)
{
    Assert(app->object_count < ArrayCount(app->object_pool));
    Assert(id < app->object_count);
    return app->object_pool + id;
}

static bool Object_IsZero(AppState *app, Object *obj)
{
    return obj == app->object_pool + 0;
}

static Object *Object_FromNetSlot(AppState *app, Uint32 network_slot)
{
    if (network_slot >= ArrayCount(app->network_ids))
        return Object_Get(app, 0);

    Uint32 id = app->network_ids[network_slot];
    return Object_Get(app, id);
}

// @info(mg) This function will probably be replaced in the future
//           when we track 'key/index' in the object itself.
static Uint32 Object_IdFromPointer(AppState *app, Object *obj)
{
    size_t byte_delta = (size_t)obj - (size_t)app->object_pool;
    size_t id = byte_delta / sizeof(*obj);
    Assert(id < ArrayCount(app->object_pool));
    return (Uint32)id;
}

static Object *Object_Create(AppState *app, Uint32 sprite_id, Uint32 flags)
{
    Assert(app->object_count < ArrayCount(app->object_pool));
    Object *obj = app->object_pool + app->object_count;
    app->object_count += 1;

    SDL_zerop(obj);
    obj->flags = flags;
    obj->sprite_id = sprite_id;
    obj->sprite_color = ColorF_RGB(1,1,1);
    return obj;
}

static Object *Object_Wall(AppState *app, V2 p, V2 dim)
{
    V2 half_dim = V2_Scale(dim, 0.5f);
    Col_Vertices collision_verts = {0};
    collision_verts.arr[0] = (V2){-half_dim.x, -half_dim.y};
    collision_verts.arr[1] = (V2){ half_dim.x, -half_dim.y};
    collision_verts.arr[2] = (V2){ half_dim.x,  half_dim.y};
    collision_verts.arr[3] = (V2){-half_dim.x,  half_dim.y};

    Sprite *sprite = Sprite_CreateNoTex(app, collision_verts);
    Object *obj = Object_Create(app, Sprite_IdFromPointer(app, sprite),
                                ObjectFlag_Draw|ObjectFlag_Collide);
    obj->p = p;

    static float r = 0.f;
    static float g = 0.5f;
    r += 0.321f;
    g += 0.111f;
    while (r > 1.f) r -= 1.f;
    while (g > 1.f) g -= 1.f;

    obj->sprite_color = ColorF_RGB(r, g, 0.5f);
    return obj;
}

typedef struct
{
    RngF arr[4];
} Col_Projection;

static Col_Projection CollisionProjection(Col_Normals normals, Col_Vertices verts)
{
    Col_Projection result = {0};

    static_assert(ArrayCount(result.arr) == ArrayCount(normals.arr));
    ForArray(normal_index, normals.arr)
    {
        RngF *projection = result.arr + normal_index;
        projection->min = FLT_MAX;
        projection->max = -FLT_MAX;

        V2 normal = normals.arr[normal_index];
        ForArray(vert_index, verts.arr)
        {
            V2 vert = verts.arr[vert_index];
            float inner = V2_Inner(normal, vert);
            projection->min = Min(inner, projection->min);
            projection->max = Max(inner, projection->max);
        }
    }

    return result;
}
