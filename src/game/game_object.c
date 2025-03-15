READ_ONLY static Object nil_object = {};

static Object *OBJ_GetNil()
{
  return &nil_object;
}

static bool OBJ_IsNil(Object *obj)
{
  return obj == &nil_object;
}

static bool OBJ_KeyMatch(OBJ_Key a, OBJ_Key b)
{
  return (a.serial_number == b.serial_number &&
          a.index == b.index);
}

static Object *OBJ_Get(OBJ_Key key, U32 storage_mask)
{
  Object *result = OBJ_GetNil();

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
    if (OBJ_KeyMatch(obj->s.key, key))
      result = obj;
  }
  return result;
}

static Object *OBJ_GetAny(OBJ_Key key)
{
  return OBJ_Get(key, ObjStorage_All);
}

static bool OBJ_SyncIsInit(OBJ_Sync *sync)
{
  return sync->init;
}

static bool OBJ_HasData(Object *obj)
{
  return !!obj->s.flags;
}

static bool OBJ_HasAnyFlag(Object *obj, U32 flag)
{
  return !!(obj->s.flags & flag);
}
static bool OBJ_HasAllFlags(Object *obj, U32 flags)
{
  return (obj->s.flags & flags) == flags;
}

static Object *OBJ_FromNetIndex(U32 net_index)
{
  if (net_index > ArrayCount(APP.net_objects))
  {
    return OBJ_GetNil();
  }

  return APP.net_objects + net_index;
}

static Object *OBJ_Create(OBJ_Storage storage, U32 flags)
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
      if (!OBJ_SyncIsInit(&search->s))
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
      if (!OBJ_SyncIsInit(&search->s))
      {
        obj = search;
        break;
      }
    }
  }

  if (!obj)
    return OBJ_GetNil();

  // init object
  SDL_zerop(obj);

  obj->s.key.serial_number = APP.obj_serial_counter;
  APP.obj_serial_counter = Max(APP.obj_serial_counter + 1, 1);

  obj->s.key.index = Checked_U64toU32(((U64)obj - (U64)APP.all_objects) / sizeof(*obj));

  obj->s.flags = flags;
  obj->s.init = true;
  obj->s.color = Color32_RGBf(1,1,1);
  return obj;
}

static Object *OBJ_CreateWall(V2 p, V2 dim, float height)
{
  Object *obj = OBJ_Create(ObjStorage_Local,
                           ObjFlag_DrawCollision | ObjFlag_Collide);
  obj->s.p = V3_From_XY_Z(p, 0);
  obj->s.collision.verts = CollisionVertices_FromRectDim(dim);
  Collision_RecalculateNormals(&obj->s.collision);
  obj->s.texture = TEX_Bricks071;
  obj->s.color = Color32_RGBf(1,1,1);
  obj->s.collision_height = height;
  return obj;
}

static Object *OBJ_CreatePlayer()
{
  Object *player = OBJ_Create(ObjStorage_Net,
                              ObjFlag_Move |
                              ObjFlag_Collide |
                              ObjFlag_DrawModel |
                              ObjFlag_AnimateRotation |
                              ObjFlag_AnimateT);

  player->s.collision.verts = CollisionVertices_FromRectDim((V2){20, 20});
  Collision_RecalculateNormals(&player->s.collision);

  player->s.model = MODEL_FemaleWorker;

  player->s.color = Color32_RGBf(1,1,1);
  player->s.animation_index = 23;
  player->s.rotation = Quat_FromAxisAngle_RH((V3){0,0,1}, -0.5f);
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
