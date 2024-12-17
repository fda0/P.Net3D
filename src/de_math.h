// ---
// Axis
// ---
typedef enum {
    Axis2_X,
    Axis2_Y,
    Axis2_COUNT
} Axis2;

static Axis2 Axis2_Other(Axis2 axis)
{
    return (axis == Axis2_X ? Axis2_Y : Axis2_X);
}

// ---
// Scalar math
// ---
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(min, max, val) (((val)<(min)) ? (min) : ((val)>(max))?(max):(val))

static float SqrtF(float a)
{
    return SDL_sqrtf(a);
}
static float AbsF(float a)
{
    return (a < 0.f ? -a : a);
}
static float SignF(float a)
{
    return (a < 0.f ? -1.f : 1.f);
}
static float SinF(float turns)
{
    // @todo custom sincos that with turn input
    return SDL_sinf(turns*2.f*SDL_PI_F);
}
static float CosF(float turns)
{
    // @todo custom sincos that with turn input
    return SDL_cosf(turns*2.f*SDL_PI_F);
}

typedef struct
{
    float sin, cos;
} SinCosResult;
static SinCosResult SinCosF(float turns)
{
    // @note most common sin/cos algorithms can provide both sin and cos
    //       "almost for free" - we just need to copypaste some good one here
    // @todo custom sincos that works with turn input
    return (SinCosResult){SinF(turns), CosF(turns)};
}

static float CeilF(float a)
{
    return SDL_ceilf(a);
}
static float FloorF(float a)
{
    return SDL_floorf(a);
}
static float TruncateF(float a)
{
    return SDL_truncf(a);
}
static float RoundF(float a)
{
    // there is also SDL_lroundf variant that rounds to nearest long
    return SDL_roundf(a);
}

static float LerpF(float a, float b, float t)
{
    return a + (b - a) * t;
}
static float ModuloF(float a, float mod)
{
    return a - TruncateF(a / mod) * mod;
}
static float WrapF(float min, float max, float a)
{
    float range = max - min;
    float offset = a - min;
    return (offset - (FloorF(offset / range) * range) + min);
}

// ---
// Vector math
// ---
typedef union
{
    struct { float x, y; };
    float E[2];
} V2;

static V2 V2_Scale(V2 a, float scale)
{
    return (V2){a.x*scale, a.y*scale};
}
static V2 V2_Add(V2 a, V2 b)
{
    return (V2){a.x + b.x, a.y + b.y};
}
static V2 V2_Sub(V2 a, V2 b)
{
    return (V2){a.x - b.x, a.y - b.y};
}
static V2 V2_Mul(V2 a, V2 b)
{
    return (V2){a.x * b.x, a.y * b.y};
}
static V2 V2_Reverse(V2 a)
{
    return (V2){-a.x, -a.y};
}
static float V2_Inner(V2 a, V2 b)
{
    return a.x*b.x + a.y*b.y;
}
static float V2_LengthSq(V2 a)
{
    return V2_Inner(a, a);
}
static float V2_Length(V2 a)
{
    return SqrtF(V2_LengthSq(a));
}
static V2 V2_Normalize(V2 a)
{
    float length = V2_Length(a);
    if (length) {
        float length_inv = 1.f / length;
        a = V2_Scale(a, length_inv);
    }
    return a;
}

static V2 V2_RotateClockwise90(V2 a)
{
    // rotation matrix
    // [ cos(-0.5pi) -sin(-0.5pi) ] [ x ]
    // [ sin(-0.5pi)  cos(-0.5pi) ] [ y ]
    float cos = 0;  // cos(-0.5pi)
    float sin = -1; // sin(-0.5pi)

    float x_prim = V2_Inner((V2){cos, -sin}, a);
    float y_prim = V2_Inner((V2){sin,  cos}, a);

    return (V2){x_prim, y_prim};
}
static V2 V2_RotateCounterclockwise90(V2 a)
{
    // rotation matrix
    // [ cos(0.5pi) -sin(0.5pi) ] [ x ]
    // [ sin(0.5pi)  cos(0.5pi) ] [ y ]
    float cos = 0; // cos(0.5pi)
    float sin = 1; // sin(0.5pi)

    float x_prim = V2_Inner((V2){cos, -sin}, a);
    float y_prim = V2_Inner((V2){sin,  cos}, a);

    return (V2){x_prim, y_prim};
}

static V2 V2_RotateSinCos(V2 a, SinCosResult sincos)
{
    float x_prim = V2_Inner((V2){sincos.cos, -sincos.sin}, a);
    float y_prim = V2_Inner((V2){sincos.sin, sincos.cos}, a);
    return (V2){x_prim, y_prim};
}
static V2 V2_Rotate(V2 a, float rot)
{
    SinCosResult sincos = SinCosF(rot);
    return V2_RotateSinCos(a, sincos);
}
static V2 V2_CalculateNormal(V2 a, V2 b)
{
    // Place vertex a at (0, 0) (turns line a--b into a vector).
    V2 vec = V2_Sub(b, a);
    // Make a direction vector out of it.
    V2 dir = V2_Normalize(vec);

    return V2_RotateClockwise90(dir);
}

static void V2_VerticesTransform(V2 *verts, Uint64 vert_count,
                                 float rotation, V2 offset)
{
    SinCosResult sincos = SinCosF(rotation);
    ForU64(i, vert_count)
    {
        V2 rotated = V2_RotateSinCos(verts[i], sincos);
        V2 moved = V2_Add(rotated, offset);
        verts[i] = moved;
    }
}

static void V2_VerticesOffset(V2 *verts, Uint64 vert_count, V2 offset)
{
    ForU64(i, vert_count)
    {
        verts[i] = V2_Add(verts[i], offset);
    }
}

static V2 V2_VerticesAverage(V2 *verts, Uint64 vert_count)
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


// ---
// Ranges
// ---
typedef union
{
    struct { float min, max; };
    float E[2];
} RngF; // Range float

static float RngF_MaxDistance(RngF a, RngF b)
{
    float d0 = b.min - a.max;
    float d1 = a.min - b.max;
    return Max(d0, d1);
}

static Uint64 HashU64(Uint64 seed, void *data, Uint64 size)
{
    // Idk where I even got this from;
    // most probably there are better cheap hash functions out there
    Uint8 *d = (Uint8 *)data;
    Uint64 res = seed;
    ForU64(i, size)
    {
        res = ((res << 5) + res) + d[i];
    }
    return res;
}

// ---
// Color
// ---
typedef union
{
    struct { float r, g, b, a; };
    float E[4];
} ColorF;
static ColorF ColorF_Normalize(ColorF f)
{
    ForArray(i, f.E)
        f.E[i] = Clamp(0.f, 1.f, f.E[i]);
    return f;
}
static ColorF ColorF_Lerp(ColorF a, ColorF b, float t)
{
    ColorF res = {};
    ForArray(i, res.E)
        res.E[i] = LerpF(a.E[i], b.E[i], t);
    return res;
}

static ColorF ColorF_RGBA(float r, float g, float b, float a)
{
    return (ColorF){r, g, b, a};
}
static ColorF ColorF_RGB(float r, float g, float b)
{
    return (ColorF){r, g, b, 1.f};
}
static ColorF ColorF_ChangeA(ColorF f, float a)
{
    f.a = a;
    return f;
}

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


