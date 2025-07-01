OBJ_MAX_NETWORK_OBJECTS :: 24;
OBJ_MAX_OFFLINE_OBJECTS :: 512;
OBJ_MAX_ALL_OBJECTS :: OBJ_MAX_NETWORK_OBJECTS + OBJ_MAX_OFFLINE_OBJECTS;
OBJ_MAX_ANIMATIONS :: 4;
OBJ_MAX_COLLIDER_VERTS :: 4;
OBJ_NIL :: Object.{};

OBJ_State :: struct
{
  offline_objects: [OBJ_MAX_OFFLINE_OBJECTS] Object;
  network_objects: [OBJ_MAX_NETWORK_OBJECTS] Object;
  #place offline_objects;
  all_objects: [OBJ_MAX_ALL_OBJECTS] Object = ---;

  // special objects
  sun: OBJ_Key;
  pathing_marker: OBJ_Key;
  pathing_marker_set: bool;
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
  index: u16;
  serial_number: u16;
};

OBJ_Flags :: enum_flags u32
{
  MOVE;
  COLLIDE;

  ANIMATE_ROTATION;
  ANIMATE_POSITION;
  ANIMATE_TRACKS;

  DRAW_MODEL;
  DRAW_COLLIDERS;
  // MOUSE_PICKING;

  HAS_HP;
  TARGETABLE;
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

  // game stats
  max_hp: float;
  hp: float;

  // game state
  is_attacking: bool;
  attack_t: float;
  hide_attack_animation_tail: bool;

  // visuals
  color: Color32; // unused?
  rotation: Quat;
  model: MODEL_Key; // used by DrawModel
  material: MATERIAL_Key; // used by DrawCollision
  height: float; // used by DrawCollision
  texture_texels_per_m: float; // used by DrawCollision

  collider: OBJ_Collider;
};

OBJ_Local :: struct
{
  // Object data that's kept on client side only
  animated_rot: Quat; // animates towards rotation
  animated_p: V3; // animates towards (V3){p.x, p.y, p.z}

  overwrite_animation_index: s32 = -1; // debug tool
  overwrite_animation_mode: ANIMATION_AdvanceMode;

  animations: [4] ANIMATION_PoseParams;
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
    sun := OBJ_Create(.OFFLINE, .DRAW_COLLIDERS);
    sun.s.material = MATERIAL_CreateKey("tex.Leather011");
    sun.s.height = 0.5;
    sun.s.collider = OBJ_ColliderFromRect(.{.5, .5});
    G.obj.sun = sun.s.key;
  }

  // Pathing marker
  {
    G.obj.pathing_marker = OBJ_Create(.OFFLINE, .ANIMATE_POSITION).s.key;
  }

  // Ground
  {
    ground := OBJ_Create(.OFFLINE, .DRAW_COLLIDERS);
    ground.s.collider = OBJ_ColliderFromRect(.{100, 100});
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
      OBJ_RotateCollider(*rotated_wall.s.collider, 0.1);
    }

    {
      flying_cube := OBJ_Create(.OFFLINE, .DRAW_COLLIDERS);
      flying_cube.s.p = V3.{0.5, 1.2, 1.};
      flying_cube.s.material = MATERIAL_CreateKey("tex.Tiles101");
      flying_cube.s.texture_texels_per_m = 1.;
      flying_cube.s.height = 0.3;
      flying_cube.s.collider = OBJ_ColliderFromRect(.{1., 1.});
    }
  }

  // blocks thing
  {
    for wall: 0..3
    {
      len := 32;
      for i: MakeRange(len)
      {
        d := 0.5;
        x := ifx wall <= 1 then i else (ifx wall == 2 then 0 else len);
        y := ifx wall >= 2 then i else (ifx wall == 0 then 0 else len);

        block := OBJ_Create(.OFFLINE, .DRAW_COLLIDERS|.COLLIDE);
        if !OBJ_IsNil(block)
        {
          block.s.p = V3.{d*x - len*0.5*d, d*y - len*0.5*d, 0};
          block.s.collider = OBJ_ColliderFromRect(.{d,d});
          block.s.height = d;
          block.s.material = MATERIAL_CreateKey("tex.Clay002");
        }
      }
    }
  }

  // Add trees
  {
    x_count := 8;
    y_count := 8;

    for y: MakeRange(y_count)
    {
      for x: MakeRange(x_count)
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

  OBJ_CreateHero(.NETWORK, MODEL_CreateKey("Dude"));
}

lerp :: (prev: OBJ_Sync, next: OBJ_Sync, t: float) -> OBJ_Sync
{
  res := prev;
  res.p = lerp(prev.p, next.p, t);
  res.desired_dp = lerp(prev.desired_dp, next.desired_dp, t);
  res.moved_dp = lerp(prev.moved_dp, next.moved_dp, t);
  res.color = lerp(prev.color, next.color, t);
  res.rotation = Slerp(prev.rotation, next.rotation, t);
  return res;
}

OBJ_KeyToColor :: (key: OBJ_Key) -> Color32
{
  byte_array := (*key).([4] u8);
  #assert(size_of(OBJ_Key) == size_of(type_of(byte_array)));
  #assert(size_of(OBJ_Key) == size_of(Color32));
  return byte_array.data.(*Color32).*;
}

OBJ_ColorToKey :: (color: Color32, from_bgra := true) -> OBJ_Key
{
  r := ((color      ) & 0xff).(u8);
  g := ((color >>  8) & 0xff).(u8);
  b := ((color >> 16) & 0xff).(u8);
  a := ((color >> 24) & 0xff).(u8);
  byte_array: [4] u8;
  if from_bgra byte_array = .[b,g,r,a];
  else         byte_array = .[r,g,b,a];

  #assert(size_of(OBJ_Key) == size_of(type_of(byte_array)));
  return byte_array.data.(*OBJ_Key).*;
}

OBJ_GetNil :: () -> *Object
{
  return *OBJ_NIL;
}

OBJ_IsNil :: (obj: *Object) -> bool
{
  return obj == *OBJ_NIL;
}

operator == :: (a: OBJ_Key, b: OBJ_Key) -> bool
{
  return (a.serial_number == b.serial_number && a.index == b.index);
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
    if obj.s.key == key then result = obj;
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
  prev_serial_number := obj.s.key.serial_number;
  Initialize(obj);

  obj.s.key.serial_number = max(prev_serial_number + 1, 1);
  obj.s.key.index = xx ((obj.(u64) - G.obj.all_objects.data.(u64)) / size_of(Object));

  obj.s.flags = flags;
  obj.s.init = true;
  obj.s.color = Color32_RGBf(1,1,1);
  return obj;
}

OBJ_CreateWall :: (p: V2, dim: V2, height: float) -> *Object
{
  obj := OBJ_Create(.OFFLINE, .DRAW_COLLIDERS|.COLLIDE);
  if !OBJ_IsNil(obj)
  {
    obj.s.p = V3.{xy = p};
    obj.s.collider = OBJ_ColliderFromRect(dim);
    obj.s.material = MATERIAL_CreateKey("tex.Bricks071");
    obj.s.texture_texels_per_m = 1;
    obj.s.color = Color32_Grayf(1);
    obj.s.height = height;
  }
  return obj;
}

OBJ_CreateHero :: (storage: OBJ_Storage, model: MODEL_Key) -> *Object
{
  obj := OBJ_Create(storage,
    .MOVE | .COLLIDE | .DRAW_MODEL |
    .ANIMATE_ROTATION | .ANIMATE_TRACKS |
    .HAS_HP | .TARGETABLE);

  if !OBJ_IsNil(obj)
  {
    dim := 0.4;
    obj.s.collider = OBJ_ColliderFromRect(.{dim,dim});
    obj.s.height = dim;
    obj.s.model = model;
    obj.s.color = Color32_Grayf(1);
    obj.s.rotation = RotationAroundAxis(AxisV3(.Z), -0.5);

    obj.s.max_hp = 100;
    obj.s.hp = 80;
  }
  return obj;
}
