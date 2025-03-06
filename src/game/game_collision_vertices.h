typedef struct
{
  V2 arr[4];
} CollisionVertices;
typedef CollisionVertices CollisionNormals;

static CollisionVertices CollisionVertices_FromRectDim(V2 dim)
{
  V2 p0 = V2_Scale(dim, -0.5f);
  V2 p1 = V2_Scale(dim, 0.5f);

  CollisionVertices result = {0};
  result.arr[0] = (V2){p1.x, p0.y}; // SE
  result.arr[1] = (V2){p1.x, p1.y}; // NE
  result.arr[2] = (V2){p0.x, p1.y}; // NW
  result.arr[3] = (V2){p0.x, p0.y}; // SW
  return result;
}

static void Vertices_Rotate(V2 *verts, U64 vert_count, float rotation)
{
  SinCosResult sincos = SinCosF(rotation);
  ForU64(i, vert_count)
    verts[i] = V2_RotateSinCos(verts[i], sincos);
}
static void Vertices_Offset(V2 *verts, U64 vert_count, V2 offset)
{
  ForU64(i, vert_count)
    verts[i] = V2_Add(verts[i], offset);
}
