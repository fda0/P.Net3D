// ---
// SDL type conversions
// ---
static SDL_Color ColorF_To_SDL_Color(ColorF f)
{
    float inv = 1.f / 255.f;
    Uint32 r = (Uint32)(f.r * inv);
    Uint32 g = (Uint32)(f.g * inv);
    Uint32 b = (Uint32)(f.b * inv);
    Uint32 a = (Uint32)(f.a * inv);

    SDL_Color res = {
        r & 0xff,
        (g & 0xff) <<  8,
        (b & 0xff) << 16,
        (a & 0xff) << 24,
    };
    return res;
}

static SDL_FColor ColorF_To_SDL_FColor(ColorF f)
{
    return (SDL_FColor){f.r, f.g, f.b, f.a};
}

static SDL_FPoint V2_To_SDL_FPoint(V2 v)
{
    return (SDL_FPoint){v.x, v.y};
}


