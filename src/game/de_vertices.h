typedef struct
{
    V2 arr[4];
} Col_Vertices;
typedef Col_Vertices Col_Normals; // @todo rename?

static Col_Vertices Vertices_FromRect(V2 p, V2 dim)
{
    V2 half_dim = V2_Scale(dim, 0.5f);
    V2 p0 = V2_Sub(p, half_dim);
    V2 p1 = V2_Add(p, half_dim);

    Col_Vertices result = {0};
    result.arr[0] = (V2){p0.x, p0.y};
    result.arr[1] = (V2){p1.x, p0.y};
    result.arr[2] = (V2){p1.x, p1.y};
    result.arr[3] = (V2){p0.x, p1.y};
    return result;
}

static void Vertices_Rotate(V2 *verts, Uint64 vert_count, float rotation)
{
    SinCosResult sincos = SinCosF(rotation);
    ForU64(i, vert_count)
        verts[i] = V2_RotateSinCos(verts[i], sincos);
}
static void Vertices_Scale(V2 *verts, Uint64 vert_count, float scale)
{
    ForU64(i, vert_count)
        verts[i] = V2_Scale(verts[i], scale);
}
static void Vertices_Offset(V2 *verts, Uint64 vert_count, V2 offset)
{
    ForU64(i, vert_count)
        verts[i] = V2_Add(verts[i], offset);
}
static void Vertices_Max(V2 *verts, Uint64 vert_count, V2 val)
{
    ForU64(i, vert_count)
    {
        verts[i].x = Max(verts[i].x, val.x);
        verts[i].y = Max(verts[i].y, val.y);
    }
}
static void Vertices_Min(V2 *verts, Uint64 vert_count, V2 val)
{
    ForU64(i, vert_count)
    {
        verts[i].x = Min(verts[i].x, val.x);
        verts[i].y = Min(verts[i].y, val.y);
    }
}

static V2 Vertices_Average(V2 *verts, Uint64 vert_count)
{
    V2 sum = {0};
    ForU64(i, vert_count)
    {
        sum = V2_Add(sum, verts[i]);
    }

    float inv = 1.f / (float)vert_count;
    V2 avg = V2_Scale(sum, inv);
    return avg;
}
