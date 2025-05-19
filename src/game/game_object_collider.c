static OBJ_Collider OBJ_GetColliderFromRect2D(V2 dim)
{
  V2 p0 = V2_Scale(dim, -0.5f);
  V2 p1 = V2_Scale(dim, 0.5f);

  OBJ_Collider result = {0};
  result.vals[0] = (V2){p1.x, p0.y}; // SE
  result.vals[1] = (V2){p1.x, p1.y}; // NE
  result.vals[2] = (V2){p0.x, p1.y}; // NW
  result.vals[3] = (V2){p0.x, p0.y}; // SW
  return result;
}

static void OBJ_RotateColliderVertices(OBJ_Collider *verts, float rotation)
{
  SinCosResult sincos = SinCosF(rotation);
  ForArray(i, verts->vals)
    verts->vals[i] = V2_RotateSinCos(verts->vals[i], sincos);
}

static void OBJ_OffsetColliderVertices(OBJ_Collider *verts, V2 offset)
{
  ForArray(i, verts->vals)
    verts->vals[i] = V2_Add(verts->vals[i], offset);
}

static void OBJ_UpdateColliderNormals(Object *obj)
{
  if (!obj->l.collider_normals_are_updated)
  {
    obj->l.collider_normals_are_updated = true;
    OBJ_Collider *verts = &obj->s.collider_vertices;
    OBJ_Collider *normals = &obj->l.collider_normals;

    U64 elem_count = ArrayCount(verts->vals);
    ForU64(index, elem_count)
    {
      U64 next_index = (index + 1) % elem_count;
      normals->vals[index] = V2_CalculateNormal(verts->vals[index], verts->vals[next_index]);
    }
  }
}

static OBJ_ColliderProjection OBJ_CalculateColliderProjection(OBJ_Collider *normals, OBJ_Collider *verts)
{
  OBJ_ColliderProjection result = {};
  static_assert(ArrayCount(result.ranges) == ArrayCount(normals->vals));

  ForArray(normal_index, normals->vals)
  {
    V2 normal = normals->vals[normal_index];

    RngF *projection = result.ranges + normal_index;
    projection->min = FLT_MAX;
    projection->max = -FLT_MAX;

    ForArray(vert_index, verts->vals)
    {
      V2 vert = verts->vals[vert_index];
      float inner = V2_Dot(normal, vert);
      projection->min = Min(inner, projection->min);
      projection->max = Max(inner, projection->max);
    }
  }

  return result;
}
