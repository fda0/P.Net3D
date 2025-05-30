OBJ_MAX_NETWORK_OBJECTS :: 24;
OBJ_MAX_OFFLINE_OBJECTS :: 512;
OBJ_MAX_ALL_OBJECTS :: OBJ_MAX_NETWORK_OBJECTS + OBJ_MAX_OFFLINE_OBJECTS;
OBJ_MAX_ANIMATIONS :: 4;
OBJ_MAX_COLLIDER_VERTS :: 4;
OBJ_NIL :: Object.{};

OBJ_State :: struct
{
  serial_counter: u32 = 1; // incremented and assigned to each new Object

  offline_objects: [OBJ_MAX_OFFLINE_OBJECTS] Object;
  network_objects: [OBJ_MAX_NETWORK_OBJECTS] Object;
  #place offline_objects;
  all_objects: [OBJ_MAX_ALL_OBJECTS] Object = ---;

  // special objects
  sun: OBJ_Key;
  pathing_marker: OBJ_Key;
  pathing_marker_set: bool;
};

OBJ_Flags :: enum_flags u32
{
  MOVE;
  COLLIDE;

  ANIMATE_ROTATION;
  ANIMATE_POSITION;
  ANIMATE_TRACKS;

  DRAW_MODEL;
  DRAW_COLLISION;
};

OBJ_Storage :: enum_flags u32
{
  // This enum is useful for filtering obj storage when doing queries.
  // A single object can have only one storage type.
  OFFLINE;
  NETWORK;

  ALL :: OFFLINE | NETWORK;
};

OBJ_Key :: struct
{
  serial_number: u32;
  index: u32;
};

OBJ_Sync :: struct
{
  // Object data that's synced with the server
  key: OBJ_Key;
  flags: OBJ_Flags;
  init: bool;
  p: V3; // XY center, Z at bottom
  desired_dp: V3; // move that Object wanted to make
  moved_dp: V3; // move that Object did after collision simulation

  // visuals
  color: Color32;
  rotation: Quat;
  model: MODEL_Key; // used by DrawModel
  material: MATERIAL_Key; // used by DrawCollision
  height: float; // used by DrawCollision
  texture_texels_per_m: float; // used by DrawCollision

  // input actions
  is_pathing: bool;
  pathing_dest_p: V2;

  collider_vertices: OBJ_Collider;
};

OBJ_Local :: struct
{
  // Object data that's kept on client side only
  animated_rot: Quat; // animates towards rotation
  animated_p: V3; // animates towards (V3){p.x, p.y, p.z}

  animation_index: u32;
  animation_t: float;

  collider_normals_are_updated: bool;
  collider_normals: OBJ_Collider;
};

Object :: struct
{
  s: OBJ_Sync;
  l: OBJ_Local;
};

OBJ_Init :: ()
{
  // Sun
  {
    sun := OBJ_Create(.OFFLINE, .DRAW_COLLISION);
    sun.s.material = MATERIAL_CreateKey("tex.Leather011");
    sun.s.height = 0.5;
    sun.s.collider_vertices = OBJ_GetColliderFromRect2D(V2.{.5, .5});
    G.obj.sun = sun.s.key;
  }

  // Pathing marker
  {
    G.obj.pathing_marker = OBJ_Create(.OFFLINE, .ANIMATE_POSITION).s.key;
  }

  // Ground
  {
    ground := OBJ_Create(.OFFLINE, .DRAW_COLLISION);
    ground.s.collider_vertices = OBJ_GetColliderFromRect2D(V2.{100, 100});
    ground.s.material = MATERIAL_CreateKey("tex.Grass004");
    ground.s.texture_texels_per_m = 1.0;
  }

  {
    thickness := 0.2;
    length := 8.0;
    off := length*0.5 - thickness*0.5;
    OBJ_CreateWall(V2.{ off, 0}, V2.{thickness, length}, 0.8);
    OBJ_CreateWall(V2.{-off, 0}, V2.{thickness, length}, 2.);
    OBJ_CreateWall(V2.{0,  off}, V2.{length, thickness}, 1.6);
    OBJ_CreateWall(V2.{0, -off}, V2.{length*0.5, thickness}, 1.2);

    {
      rotated_wall := OBJ_CreateWall(V2.{0.75*off, -0.5*off}, V2.{thickness, 5.*thickness}, 1);
      OBJ_RotateCollider(*rotated_wall.s.collider_vertices, 0.1);
    }

    {
      flying_cube := OBJ_Create(.OFFLINE, .DRAW_COLLISION);
      flying_cube.s.p = V3.{0.5, 1.2, 1.};
      flying_cube.s.material = MATERIAL_CreateKey("tex.Tiles101");
      flying_cube.s.texture_texels_per_m = 1.;
      flying_cube.s.height = 0.3;
      flying_cube.s.collider_vertices = OBJ_GetColliderFromRect2D(V2.{1., 1.});
    }
  }

  // blocks thing
  {
    for wall: 0..3
    {
      len := 32;
      for i: 0..len-1
      {
        d := 0.5;
        x := ifx wall <= 1 then i else (ifx wall == 2 then 0 else len);
        y := ifx wall >= 2 then i else (ifx wall == 0 then 0 else len);

        block := OBJ_Create(.OFFLINE, .DRAW_COLLISION|.COLLIDE);
        if !OBJ_IsNil(block)
        {
          block.s.p = V3.{d*x - len*0.5*d, d*y - len*0.5*d, 0};
          OBJ_SetColliderFromCube(block, V3.{d,d,d});
          block.s.material = MATERIAL_CreateKey("tex.Clay002");
        }
      }
    }
  }

  // Add trees
  {
    x_count := 8;
    y_count := 8;

    for y: 0..y_count-1
    {
      for x: 0..x_count-1
      {
        x_half_count := x_count/2;
        y_half_count := y_count/2;

        pos: V3;
        if ((x^y)&1) pos.x = 4.70;
        else         pos.y = 6.90;

        pos.x += 4.0*(x % x_half_count);
        pos.y += 2.4*(y % y_half_count);

        pos.x *= ifx x < x_half_count then -1 else 1;
        pos.y *= ifx y < y_half_count then -1 else 1;

        tree := OBJ_Create(.OFFLINE, .DRAW_MODEL);
        if !OBJ_IsNil(tree)
        {
          tree.s.p = pos;
          tree.s.model = MODEL_CreateKey("Tree");
        }
      }
    }
  }
}

OBJ_GetNil :: () -> *Object
{
  return *OBJ_NIL;
}

OBJ_IsNil :: (obj: *Object) -> bool
{
  return obj == *OBJ_NIL;
}

OBJ_KeyMatch :: (a: OBJ_Key, b: OBJ_Key) -> bool
{
  return (a.serial_number == b.serial_number &&
          a.index == b.index);
}

OBJ_Get :: (key: OBJ_Key, storage_mask: OBJ_Storage) -> *Object
{
  result := OBJ_GetNil();

  index_in_valid_storage := false;
  {
    min: u32 = 0;
    max: u32 = OBJ_MAX_OFFLINE_OBJECTS;
    if (storage_mask & .OFFLINE)
      index_in_valid_storage |= (key.index >= min && key.index < max);

    min = max;
    max += OBJ_MAX_NETWORK_OBJECTS;
    if (storage_mask & .NETWORK)
      index_in_valid_storage |= (key.index >= min && key.index < max);
  }

  if index_in_valid_storage
  {
    obj := *G.obj.all_objects[key.index];
    if OBJ_KeyMatch(obj.s.key, key)
      result = obj;
  }
  return result;
}

OBJ_GetAny :: (key: OBJ_Key) -> *Object
{
  return OBJ_Get(key, .ALL);
}

OBJ_SyncIsInit :: (sync: OBJ_Sync) -> bool
{
  return sync.init;
}

OBJ_HasData :: (obj: Object) -> bool
{
  return !!obj.s.flags;
}

OBJ_HasAnyFlag :: (obj: Object, flag: OBJ_Flags) -> bool
{
  return !!(obj.s.flags & flag);
}
OBJ_HasAllFlags :: (obj: Object, flags: OBJ_Flags) -> bool
{
  return (obj.s.flags & flags) == flags;
}

OBJ_FromNetIndex :: (net_index: u32) -> *Object
{
  if net_index > G.obj.network_objects.count
    return OBJ_GetNil();

  return *G.obj.network_objects[net_index];
}

OBJ_Create :: (storage: OBJ_Storage, flags: OBJ_Flags) -> *Object
{
  obj: *Object;
  matched_storages := 0;

  if storage & .OFFLINE
  {
    matched_storages += 1;
    for *G.obj.offline_objects
    {
      if !OBJ_SyncIsInit(it.s)
      {
        obj = it;
        break;
      }
    }
  }
  if storage & .NETWORK
  {
    matched_storages += 1;
    for *G.obj.network_objects
    {
      if !OBJ_SyncIsInit(it.s)
      {
        obj = it;
        break;
      }
    }
  }

  if matched_storages != 1
  {
    obj = null;
    assert(matched_storages == 1,
      tprint("Invalid OBJ_Storage (%) used in OBJ_Create", storage));
  }
  if (!obj)
    return OBJ_GetNil();

  // init object
  obj.* = .{};

  obj.s.key.serial_number = G.obj.serial_counter;
  G.obj.serial_counter = max(G.obj.serial_counter + 1, 1);
  obj.s.key.index = cast(u32)((cast(u64)obj - cast(u64)*G.obj.all_objects[0]) / size_of(Object));

  obj.s.flags = flags;
  obj.s.init = true;
  obj.s.color = Color32_RGBf(1,1,1);
  return obj;
}

OBJ_CreateWall :: (p: V2, dim: V2, height: float) -> *Object
{
  obj := OBJ_Create(.OFFLINE, .DRAW_COLLISION|.COLLIDE);
  if !OBJ_IsNil(obj)
  {
    obj.s.p = V3.{xy = p};
    obj.s.collider_vertices = OBJ_GetColliderFromRect2D(dim);
    obj.s.material = MATERIAL_CreateKey("tex.Bricks071");
    obj.s.texture_texels_per_m = 1;
    obj.s.color = Color32_Grayf(1);
    obj.s.height = height;
  }
  return obj;
}

OBJ_CreatePlayer :: (model: MODEL_Key) -> *Object
{
  obj := OBJ_Create(.NETWORK,
    .MOVE | .COLLIDE | .DRAW_MODEL|
    .ANIMATE_ROTATION | .ANIMATE_TRACKS);

  if !OBJ_IsNil(obj)
  {
    dim := 0.4;
    OBJ_SetColliderFromCube(obj, V3.{dim,dim,dim});

    obj.s.model = model;
    obj.s.color = Color32_Grayf(1);
    obj.s.rotation = RotationAroundAxis(ToVec(.Z), -0.5);
  }
  return obj;
}
