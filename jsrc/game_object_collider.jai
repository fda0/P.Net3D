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

  result = OBJ_Collider.{};
  result.vals[0] = V2.{p1.x, p0.y}; // SE
  result.vals[1] = V2.{p1.x, p1.y}; // NE
  result.vals[2] = V2.{p0.x, p1.y}; // NW
  result.vals[3] = V2.{p0.x, p0.y}; // SW
  return result;
}

OBJ_SetColliderFromCube :: (obj: *Object, dim: V3)
{
  obj.s.collider_vertices = OBJ_GetColliderFromRect2D(V2_FromV3_XY(dim));
  obj.l.collider_normals_are_updated = false;
  obj.s.height = dim.z;
}

OBJ_RotateCollider :: (collider: *OBJ_Collider, rotation: float)
{
  sin, cos := SinCos(rotation);
  for *collider.values
    it.* = V2_RotateSinCos(it.*, sin, cos);
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
    for 0..elem_count-1
    {
      next_it := (it + 1) % elem_count;
      normals.values[it]= V2_CalculateNormal(verts.values[it], verts.values[next_it]);
    }
  }
}

OBJ_CalculateColliderProjection :: (normals: *OBJ_Collider, verts: *OBJ_Collider) -> OBJ_ColliderProjection
{
  result = OBJ_ColliderProjection.{};
  for normal: normals.values
  {
    projection: *result.ranges[it_index];
    projection.min = FLT_MAX;
    projection.max = -FLT_MAX;

    for vert: verts.values
    {
      inner := dot(normal, vert);
      projection.min = min(inner, projection.min);
      projection.max = max(inner, projection.max);
    }
  }
  return result;
}
