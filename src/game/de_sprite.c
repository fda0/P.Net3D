// @info(mg) This function will probably be replaced in the future
//           when we track 'key/index' in the sprite itself
//           (would be useful for dynamic in/out streaming of sprites).
static Uint32 Sprite_IdFromPointer(AppState *app, Sprite *sprite)
{
    size_t byte_delta = (size_t)sprite - (size_t)app->sprite_pool;
    size_t id = byte_delta / sizeof(*sprite);
    Assert(id < ArrayCount(app->sprite_pool));
    return (Uint32)id;
}

static Sprite *Sprite_Get(AppState *app, Uint32 sprite_id)
{
    Assert(sprite_id < ArrayCount(app->sprite_pool));
    return app->sprite_pool + sprite_id;
}

// @todo delete these helpers?
static void Sprite_CollisionVerticesRotate(Sprite *sprite, float rotation)
{
    Vertices_Rotate(sprite->collision_vertices.arr,
                      ArrayCount(sprite->collision_vertices.arr),
                      rotation);
}
static void Sprite_CollisionVerticesScale(Sprite *sprite, float scale)
{
    Vertices_Scale(sprite->collision_vertices.arr,
                      ArrayCount(sprite->collision_vertices.arr),
                      scale);
}
static void Sprite_CollisionVerticesOffset(Sprite *sprite, V2 offset)
{
    Vertices_Offset(sprite->collision_vertices.arr,
                      ArrayCount(sprite->collision_vertices.arr),
                      offset);
}
static void Sprite_CollisionVerticesMax(Sprite *sprite, V2 val)
{
    Vertices_Max(sprite->collision_vertices.arr,
                   ArrayCount(sprite->collision_vertices.arr),
                   val);
}
static void Sprite_CollisionVerticesMin(Sprite *sprite, V2 val)
{
    Vertices_Min(sprite->collision_vertices.arr,
                   ArrayCount(sprite->collision_vertices.arr),
                   val);
}

static void Sprite_RecalculateCollsionNormals(Sprite *sprite)
{
    Uint64 vert_count = ArrayCount(sprite->collision_vertices.arr);
    ForU64(vert_id, vert_count)
    {
        Uint64 next_vert_id = vert_id + 1;
        if (next_vert_id >= vert_count)
            next_vert_id -= vert_count;

        sprite->collision_normals.arr[vert_id]
            = V2_CalculateNormal(sprite->collision_vertices.arr[vert_id],
                                 sprite->collision_vertices.arr[next_vert_id]);
    }
}

static void Sprite_UpdateCollisionVertices(Sprite *sprite, CollisionVertices collision_vertices)
{
    sprite->collision_vertices = collision_vertices;
    Sprite_RecalculateCollsionNormals(sprite);
}

static Sprite *Sprite_CreateNoTex(AppState *app, CollisionVertices collision_vertices)
{
    Assert(app->sprite_count < ArrayCount(app->sprite_pool));
    Sprite *sprite = app->sprite_pool + app->sprite_count;
    app->sprite_count += 1;

    Sprite_UpdateCollisionVertices(sprite, collision_vertices);
    return sprite;
}

static Sprite *Sprite_Create(AppState *app, const char *texture_path, Uint32 tex_frames)
{
    if (tex_frames == 0)
        tex_frames = 1;

#if 0
    SDL_Texture *tex = IMG_LoadTexture(app->renderer, texture_path);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    V2 tex_half_dim = {(float)tex->w, (float)tex->h};
#else
    (void)texture_path;
    SDL_Texture *tex = 0;

    V2 tex_half_dim = {50, 20};
#endif

    tex_half_dim.y /= (float)tex_frames;
    tex_half_dim = V2_Scale(tex_half_dim, 0.5f);

    CollisionVertices default_col_verts = {0};
    default_col_verts.arr[0] = (V2){-tex_half_dim.x, -tex_half_dim.y};
    default_col_verts.arr[1] = (V2){ tex_half_dim.x, -tex_half_dim.y};
    default_col_verts.arr[2] = (V2){ tex_half_dim.x,  tex_half_dim.y};
    default_col_verts.arr[3] = (V2){-tex_half_dim.x,  tex_half_dim.y};

    Sprite *sprite = Sprite_CreateNoTex(app, default_col_verts);
    sprite->tex = tex;
    sprite->tex_frames = tex_frames;
    return sprite;
}
