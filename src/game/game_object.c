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
    return (a.serial_number == b.serial_number &&
            a.index == b.index);
}

static Object *Obj_Get(Obj_Key key, U32 storage_mask)
{
    Object *result = Obj_GetNil();

    bool index_in_valid_storage = false;
    {
        U32 min = 0;
        U32 max = OBJ_MAX_CONST_OBJECTS;
        if (storage_mask & ObjStorage_Local)
            index_in_valid_storage |= (key.index >= min && key.index < max);

        min = max;
        max += OBJ_MAX_NETWORK_OBJECTS;
        if (storage_mask & ObjStorage_Net)
            index_in_valid_storage |= (key.index >= min && key.index < max);
    }

    if (index_in_valid_storage)
    {
        Object *obj = APP.all_objects + key.index;
        if (Obj_KeyMatch(obj->s.key, key))
            result = obj;
    }
    return result;
}

static bool Obj_SyncIsInit(Obj_Sync *sync)
{
    return sync->init;
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

static Object *Obj_Create(Obj_Category storage, U32 flags)
{
    bool matched_storage = false;
    Object *obj = 0;

    if (storage & ObjStorage_Local)
    {
        Assert(!matched_storage);
        matched_storage = true;

        ForArray(i, APP.const_objects)
        {
            Object *search = APP.const_objects + i;
            if (!Obj_SyncIsInit(&search->s))
            {
                obj = search;
                break;
            }
        }
    }
    if (storage & ObjStorage_Net)
    {
        Assert(!matched_storage);
        matched_storage = true;

        ForArray(i, APP.net_objects)
        {
            Object *search = APP.net_objects + i;
            if (!Obj_SyncIsInit(&search->s))
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

    obj->s.key.serial_number = APP.obj_serial_counter;
    APP.obj_serial_counter = Max(APP.obj_serial_counter + 1, 1);

    obj->s.key.index = Checked_U64toU32(((U64)obj - (U64)APP.all_objects) / sizeof(*obj));

    obj->s.flags = flags;
    obj->s.init = true;
    obj->s.color = ColorF_RGB(1,1,1);
    return obj;
}

static Object *Obj_CreateWall(V2 p, V2 dim)
{
    Object *obj = Obj_Create(ObjStorage_Local,
                             ObjFlag_Draw|ObjFlag_Collide);
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
    Object *player = Obj_Create(ObjStorage_Net,
                                ObjFlag_Draw | ObjFlag_Move |
                                ObjFlag_Collide |
                                ObjFlag_ModelTeapot |
                                ObjFlag_AnimateRotation);

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
