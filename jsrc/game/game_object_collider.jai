OBJ_Collider :: struct
{
  values: [OBJ_MAX_COLLIDER_VERTS] V2;
};

OBJ_ColliderProjection :: struct
{
  ranges: [OBJ_MAX_COLLIDER_VERTS] Range(float);
};

OBJ_GetColliderFromRect2D :: (dim: V2) -> OBJ_Collider
{
  p0 := dim * -0.5;
  p1 := dim * 0.5;

  res: OBJ_Collider;
  res.values[0] = .{p1.x, p0.y}; // SE
  res.values[1] = .{p1.x, p1.y}; // NE
  res.values[2] = .{p0.x, p1.y}; // NW
  res.values[3] = .{p0.x, p0.y}; // SW
  return res;
}

OBJ_SetColliderFromCube :: (obj: *Object, dim: V3)
{
  obj.s.collider_vertices = OBJ_GetColliderFromRect2D(dim.xy);
  obj.l.collider_normals_are_updated = false;
  obj.s.height = dim.z;
}

OBJ_RotateCollider :: (collider: *OBJ_Collider, turns: float)
{
  for *collider.values
    it.* = Rotate(it.*, turns);
}

OBJ_OffsetCollider :: (collider: *OBJ_Collider, offset: V2)
{
  for *collider.values
    it.* += offset;
}

OBJ_UpdateColliderNormals :: (obj: *Object)
{
  if !obj.l.collider_normals_are_updated
  {
    obj.l.collider_normals_are_updated = true;
    verts := *obj.s.collider_vertices;
    normals := *obj.l.collider_normals;

    elem_count := verts.values.count;
    for MakeRange(elem_count)
    {
      next_it := (it + 1) % elem_count;
      normals.values[it] = NormalFromSegment(verts.values[it], verts.values[next_it]);
    }
  }
}

OBJ_CalculateColliderProjection :: (normals: *OBJ_Collider, verts: *OBJ_Collider) -> OBJ_ColliderProjection
{
  res: OBJ_ColliderProjection;
  for normal: normals.values
  {
    projection := *res.ranges[it_index];
    projection.min = FLOAT32_MAX;
    projection.max = -FLOAT32_MAX;

    for vert: verts.values
    {
      inner := dot(normal, vert);
      projection.min = min(inner, projection.min);
      projection.max = max(inner, projection.max);
    }
  }
  return res;
}
