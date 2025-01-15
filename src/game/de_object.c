READ_ONLY static Object nil_object = {};

static Object *Object_GetNil()
{
    return &nil_object;
}

static bool Object_IsNil(Object *obj)
{
    return obj == &nil_object;
}

static bool Object_KeyMatch(Object_Key a, Object_Key b)
{
    return (a.made_at_tick == b.made_at_tick &&
            a.index == b.index);
}

static Object *Object_Get(AppState *app, Object_Key key, Uint32 category_mask)
{
    Object *result = Object_GetNil();

    bool index_in_valid_category = false;
    {
        Uint32 min = 0;
        Uint32 max = OBJ_MAX_CONST_OBJECTS;
        if (category_mask & ObjCategory_Local)
            index_in_valid_category |= (key.index >= min && key.index < max);

        min = max;
        max += OBJ_MAX_NETWORK_OBJECTS;
        if (category_mask & ObjCategory_Net)
            index_in_valid_category |= (key.index >= min && key.index < max);
    }

    if (index_in_valid_category)
    {
        Object *obj = app->all_objects + key.index;
        if (Object_KeyMatch(obj->key, key))
            result = obj;
    }
    return result;
}

static bool Object_IsInit(Object *obj)
{
    return obj->init;
}

static bool Object_HasData(Object *obj)
{
    return !!obj->flags;
}

static bool Object_HasAnyFlag(Object *obj, Uint32 flag)
{
    return !!(obj->flags & flag);
}
static bool Object_HasAllFlags(Object *obj, Uint32 flags)
{
    return (obj->flags & flags) == flags;
}

static Object *Object_FromNetIndex(AppState *app, Uint32 net_index)
{
    if (net_index > ArrayCount(app->net_objects))
    {
        return Object_GetNil();
    }

    return app->net_objects + net_index;
}

static Object *Object_Create(AppState *app, Obj_Category category, Uint32 sprite_id, Uint32 flags)
{
    bool matched_category = false;
    Object *obj = 0;

    if (category & ObjCategory_Local)
    {
        Assert(!matched_category);
        matched_category = true;

        ForArray(i, app->const_objects)
        {
            Object *search = app->const_objects + i;
            if (!Object_IsInit(search))
            {
                obj = search;
                break;
            }
        }
    }
    if (category & ObjCategory_Net)
    {
        Assert(!matched_category);
        matched_category = true;

        ForArray(i, app->net_objects)
        {
            Object *search = app->net_objects + i;
            if (!Object_IsInit(search))
            {
                obj = search;
                break;
            }
        }
    }

    if (!obj)
        return Object_GetNil();

    // init object
    SDL_zerop(obj);

    obj->key.made_at_tick = app->tick_id;
    obj->key.index = ((Uint64)obj - (Uint64)app->all_objects) / sizeof(*obj);

    obj->flags = flags;
    obj->init = true;
    obj->sprite_id = sprite_id;
    obj->sprite_color = ColorF_RGB(1,1,1);
    return obj;
}

static Object *Object_CreateWall(AppState *app, V2 p, V2 dim)
{
    V2 half_dim = V2_Scale(dim, 0.5f);
    Col_Vertices collision_verts = {0};
    collision_verts.arr[0] = (V2){-half_dim.x, -half_dim.y};
    collision_verts.arr[1] = (V2){ half_dim.x, -half_dim.y};
    collision_verts.arr[2] = (V2){ half_dim.x,  half_dim.y};
    collision_verts.arr[3] = (V2){-half_dim.x,  half_dim.y};

    Sprite *sprite = Sprite_CreateNoTex(app, collision_verts);
    Object *obj = Object_Create(app, ObjCategory_Local,
                                Sprite_IdFromPointer(app, sprite),
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

static Object *Object_CreatePlayer(AppState *app)
{
    Object *player = Object_Create(app, ObjCategory_Net,
                                   app->sprite_dude_id,
                                   ObjectFlag_Draw|ObjectFlag_Move /*|ObjectFlag_Collide*/ );
    player->sprite_color = ColorF_RGB(1, 0.1f, 0.1f);
    return player;
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
