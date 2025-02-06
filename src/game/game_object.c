READ_ONLY static Object nil_object = {};

static Object *Obj_GetNil()
{
    return &nil_object;
}

static bool Obj_IsNil(Object *obj)
{
    return obj == &nil_object;
}

static bool Obj_KeyMatch(Obj_Key a, Obj_Key b)
{
    return (a.made_at_tick == b.made_at_tick &&
            a.index == b.index);
}

static Object *Obj_Get(Obj_Key key, U32 category_mask)
{
    Object *result = Obj_GetNil();

    bool index_in_valid_category = false;
    {
        U32 min = 0;
        U32 max = OBJ_MAX_CONST_OBJECTS;
        if (category_mask & ObjCategory_Local)
            index_in_valid_category |= (key.index >= min && key.index < max);

        min = max;
        max += OBJ_MAX_NETWORK_OBJECTS;
        if (category_mask & ObjCategory_Net)
            index_in_valid_category |= (key.index >= min && key.index < max);
    }

    if (index_in_valid_category)
    {
        Object *obj = APP.all_objects + key.index;
        if (Obj_KeyMatch(obj->s.key, key))
            result = obj;
    }
    return result;
}

static bool Obj_IsInit(Object *obj)
{
    return obj->s.init;
}

static bool Obj_HasData(Object *obj)
{
    return !!obj->s.flags;
}

static bool Obj_HasAnyFlag(Object *obj, U32 flag)
{
    return !!(obj->s.flags & flag);
}
static bool Obj_HasAllFlags(Object *obj, U32 flags)
{
    return (obj->s.flags & flags) == flags;
}

static Object *Obj_FromNetIndex(U32 net_index)
{
    if (net_index > ArrayCount(APP.net_objects))
    {
        return Obj_GetNil();
    }

    return APP.net_objects + net_index;
}

static Object *Obj_Create(Obj_Category category, U32 flags)
{
    bool matched_category = false;
    Object *obj = 0;

    if (category & ObjCategory_Local)
    {
        Assert(!matched_category);
        matched_category = true;

        ForArray(i, APP.const_objects)
        {
            Object *search = APP.const_objects + i;
            if (!Obj_IsInit(search))
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

        ForArray(i, APP.net_objects)
        {
            Object *search = APP.net_objects + i;
            if (!Obj_IsInit(search))
            {
                obj = search;
                break;
            }
        }
    }

    if (!obj)
        return Obj_GetNil();

    // init object
    SDL_zerop(obj);

    obj->s.key.made_at_tick = APP.tick_id;
    obj->s.key.index = ((U64)obj - (U64)APP.all_objects) / sizeof(*obj);

    obj->s.flags = flags;
    obj->s.init = true;
    obj->s.color = ColorF_RGB(1,1,1);
    return obj;
}

static Object *Obj_CreateWall(V2 p, V2 dim)
{
    Object *obj = Obj_Create(ObjCategory_Local,
                                ObjectFlag_Draw|ObjectFlag_Collide);
    obj->s.p = p;
    obj->s.collision.verts = CollisionVertices_FromRectDim(dim);
    Collision_RecalculateNormals(&obj->s.collision);

    static float r = 0.f;
    static float g = 0.5f;
    r += 0.321f;
    g += 0.111f;
    while (r > 1.f) r -= 1.f;
    while (g > 1.f) g -= 1.f;

    obj->s.color = ColorF_RGB(r, g, 0.5f);
    return obj;
}

static Object *Obj_CreatePlayer()
{
    Object *player = Obj_Create(ObjCategory_Net,
                                   ObjectFlag_Draw|ObjectFlag_Move|
                                   /*|ObjectFlag_Collide*/
                                   ObjectFlag_ModelTeapot |
                                   ObjectFlag_AnimateRotation);

    player->s.collision.verts = CollisionVertices_FromRectDim((V2){30, 30});
    Collision_RecalculateNormals(&player->s.collision);

    player->s.color = ColorF_RGB(1,1,1);
    return player;
}


typedef struct
{
    RngF arr[4];
} CollisionProjection;

static CollisionProjection Collision_CalculateProjection(CollisionNormals normals, CollisionVertices verts)
{
    CollisionProjection result = {0};

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

static void Collision_RecalculateNormals(Collision_Data *collision)
{
    U64 vert_count = ArrayCount(collision->verts.arr);
    ForU64(vert_id, vert_count)
    {
        U64 next_vert_id = vert_id + 1;
        if (next_vert_id >= vert_count)
            next_vert_id -= vert_count;

        collision->norms.arr[vert_id]
            = V2_CalculateNormal(collision->verts.arr[vert_id],
                                 collision->verts.arr[next_vert_id]);
    }
}
