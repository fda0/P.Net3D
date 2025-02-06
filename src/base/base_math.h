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
// Vector
// ---
typedef union
{
    struct { float x, y; };
    float E[2];
} V2;

typedef union
{
    struct { float x, y, z; };
    float E[3];
} V3;

typedef union
{
    struct { float x, y, z, w; };
    float E[4];
} V4;

typedef V4 Quat;

// ---
// Scalar math
// ---
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(min, max, val) (((val)<(min)) ? (min) : ((val)>(max))?(max):(val))

#define TURNS_TO_RAD (2.f*SDL_PI_F)
#define RAD_TO_TURNS (1.f/(2.f*SDL_PI_F))

static float SqrtF(float a)
{
    return SDL_sqrtf(a);
}
static float InvSqrtF(float a)
{
    // @todo use SSE intrinsic?
    return 1.f / SqrtF(a);
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
    return SDL_sinf(turns*TURNS_TO_RAD);
}
static float CosF(float turns)
{
    // @todo custom sincos that with turn input
    return SDL_cosf(turns*TURNS_TO_RAD);
}
static float TanF(float turns)
{
    // @todo custom tan that with turn input
    return SDL_tanf(turns*TURNS_TO_RAD);
}
static float AsinF(float value)
{
    return SDL_asinf(value)*RAD_TO_TURNS;
}
static float AcosF(float value)
{
    return SDL_acosf(value)*RAD_TO_TURNS;
}
static float Atan2F(V2 vec)
{
    return SDL_atan2f(vec.x, vec.y)*RAD_TO_TURNS;
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

static void *AlignPointerUp(void *ptr, U64 alignment) {
    U64 pointer = (U64)ptr;
    pointer += (alignment - 1);
    pointer = pointer & (~((alignment - 1)));
    return (void *)pointer;
}

// ---
// Vector math
// ---
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
static V2 V2_NanToZero(V2 a)
{
    if (a.x != a.x) a.x = 0.f;
    if (a.y != a.y) a.y = 0.f;
    return a;
}
static V2 V2_Lerp(V2 a, V2 b, float t)
{
    return (V2){LerpF(a.x, b.x, t), LerpF(a.y, b.y, t)};
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
    float len_sq = V2_LengthSq(a);
    if (len_sq) {
        float len_inv = InvSqrtF(len_sq);
        a = V2_Scale(a, len_inv);
    }
    return a;
}

//
// V3
//
static V3 V3_Make_XY_Z(V2 xy, float z)
{
    return (V3){xy.x, xy.y, z};
}
static V3 V3_Make_XZ_Y(V2 xz, float y)
{
    return (V3){xz.x, y, xz.y};
}
static V3 V3_Make_YX_Z(V2 xy, float z)
{
    return (V3){xy.y, xy.x, z};
}
static V3 V3_Make_ZY_X(V2 zy, float x)
{
    return (V3){zy.x, zy.y, x};
}

static V3 V3_Scale(V3 a, float scale)
{
    return (V3){a.x*scale, a.y*scale, a.z*scale};
}
static V3 V3_Add(V3 a, V3 b)
{
    return (V3){a.x + b.x, a.y + b.y, a.z + b.z};
}
static V3 V3_Sub(V3 a, V3 b)
{
    return (V3){a.x - b.x, a.y - b.y, a.z - b.z};
}
static V3 V3_Mul(V3 a, V3 b)
{
    return (V3){a.x * b.x, a.y * b.y, a.z * b.z};
}
static V3 V3_Reverse(V3 a)
{
    return (V3){-a.x, -a.y, -a.z};
}
static float V3_Inner(V3 a, V3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
static V3 V3_Cross(V3 a, V3 b)
{
    V3 res =
    {
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x),
    };
    return res;
}
static V3 V3_NanToZero(V3 a)
{
    if (a.x != a.x) a.x = 0.f;
    if (a.y != a.y) a.y = 0.f;
    if (a.z != a.z) a.z = 0.f;
    return a;
}
static V3 V3_Lerp(V3 a, V3 b, float t)
{
    return (V3){LerpF(a.x, b.x, t), LerpF(a.y, b.y, t), LerpF(a.z, b.z, t)};
}
static float V3_LengthSq(V3 a)
{
    return V3_Inner(a, a);
}
static float V3_Length(V3 a)
{
    return SqrtF(V3_LengthSq(a));
}
static V3 V3_Normalize(V3 a)
{
    float len_sq = V3_LengthSq(a);
    if (len_sq) {
        float len_inv = InvSqrtF(len_sq);
        a = V3_Scale(a, len_inv);
    }
    return a;
}

//
// V4
//
static V4 V4_Scale(V4 a, float scale)
{
    return (V4){a.x*scale, a.y*scale, a.z*scale, a.w*scale};
}
static V4 V4_Add(V4 a, V4 b)
{
    return (V4){a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
static V4 V4_Sub(V4 a, V4 b)
{
    return (V4){a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
static V4 V4_Mul(V4 a, V4 b)
{
    return (V4){a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}
static V4 V4_Reverse(V4 a)
{
    return (V4){-a.x, -a.y, -a.z, -a.w};
}
static float V4_Inner(V4 a, V4 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}
static V4 V4_NanToZero(V4 a)
{
    if (a.x != a.x) a.x = 0.f;
    if (a.y != a.y) a.y = 0.f;
    if (a.z != a.z) a.z = 0.f;
    if (a.w != a.w) a.w = 0.f;
    return a;
}
static V4 V4_Lerp(V4 a, V4 b, float t)
{
    return (V4){LerpF(a.x, b.x, t), LerpF(a.y, b.y, t), LerpF(a.z, b.z, t), LerpF(a.w, b.w, t)};
}
static float V4_LengthSq(V4 a)
{
    return V4_Inner(a, a);
}
static float V4_Length(V4 a)
{
    return SqrtF(V4_LengthSq(a));
}
static V4 V4_Normalize(V4 a)
{
    float len_sq = V4_LengthSq(a);
    if (len_sq) {
        float len_inv = InvSqrtF(len_sq);
        a = V4_Scale(a, len_inv);
    }
    return a;
}

// Additional V2 math
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


// ---
// Ranges
// ---
typedef union
{
    struct { U32 min, max; };
    U32 E[2];
} RngU32;

typedef union
{
    struct { U64 min, max; };
    U64 E[2];
} RngU64;

typedef union
{
    struct { float min, max; };
    float E[2];
} RngF; // Range float


static bool RngU32_InRange(RngU32 rng, U32 value)
{
    return rng.min >= value && rng.max < value;
}

static U64 RngU64_Count(RngU64 rng)
{
    if (rng.max < rng.min)
        return 0;
    return rng.max - rng.min;
}

static float RngF_MaxDistance(RngF a, RngF b)
{
    float d0 = b.min - a.max;
    float d1 = a.min - b.max;
    return Max(d0, d1);
}

// ---
// Color
// ---
typedef union
{
    struct { float r, g, b, a; };
    float E[4];
} ColorF;
static ColorF ColorF_Clamp(ColorF f)
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
// Matrix
// Heavy duty matrix math funcs were ported from HandmadeMath.h
// ---
typedef union
{
    // col-major
    // 0 4 8 c
    // 1 5 9 d
    // 2 6 a e
    // 3 7 b f
    float elem[4][4]; // [x -> col][y -> row]
    float flat[16];
    V4 cols[4];
} Mat4;

static Mat4 Mat4_Diagonal(float value)
{
    Mat4 res = {0};
    res.elem[0][0] = value;
    res.elem[1][1] = value;
    res.elem[2][2] = value;
    res.elem[3][3] = value;
    return res;
}

static Mat4 Mat4_Identity()
{
    return Mat4_Diagonal(1.f);
}

static Mat4 Mat4_Transpose(Mat4 mat)
{
    // @todo sse
    Mat4 res;
    res.elem[0][0] = mat.elem[0][0];
    res.elem[0][1] = mat.elem[1][0];
    res.elem[0][2] = mat.elem[2][0];
    res.elem[0][3] = mat.elem[3][0];
    res.elem[1][0] = mat.elem[0][1];
    res.elem[1][1] = mat.elem[1][1];
    res.elem[1][2] = mat.elem[2][1];
    res.elem[1][3] = mat.elem[3][1];
    res.elem[2][0] = mat.elem[0][2];
    res.elem[2][1] = mat.elem[1][2];
    res.elem[2][2] = mat.elem[2][2];
    res.elem[2][3] = mat.elem[3][2];
    res.elem[3][0] = mat.elem[0][3];
    res.elem[3][1] = mat.elem[1][3];
    res.elem[3][2] = mat.elem[2][3];
    res.elem[3][3] = mat.elem[3][3];
    return res;
}

static V4 V4_MulM4(Mat4 mat, V4 vec)
{
    // @todo SIMD
    V4 res;
    res.x = vec.E[0] * mat.cols[0].x;
    res.y = vec.E[0] * mat.cols[0].y;
    res.z = vec.E[0] * mat.cols[0].z;
    res.w = vec.E[0] * mat.cols[0].w;

    res.x += vec.E[1] * mat.cols[1].x;
    res.y += vec.E[1] * mat.cols[1].y;
    res.z += vec.E[1] * mat.cols[1].z;
    res.w += vec.E[1] * mat.cols[1].w;

    res.x += vec.E[2] * mat.cols[2].x;
    res.y += vec.E[2] * mat.cols[2].y;
    res.z += vec.E[2] * mat.cols[2].z;
    res.w += vec.E[2] * mat.cols[2].w;

    res.x += vec.E[3] * mat.cols[3].x;
    res.y += vec.E[3] * mat.cols[3].y;
    res.z += vec.E[3] * mat.cols[3].z;
    res.w += vec.E[3] * mat.cols[3].w;
    return res;
}

static Mat4 Mat4_Mul(Mat4 left, Mat4 right)
{
    Mat4 res;
    res.cols[0] = V4_MulM4(left, right.cols[0]);
    res.cols[1] = V4_MulM4(left, right.cols[1]);
    res.cols[2] = V4_MulM4(left, right.cols[2]);
    res.cols[3] = V4_MulM4(left, right.cols[3]);
    return res;
}

static Mat4 Mat4_Rotation_RH(V3 axis, float turns)
{
    axis = V3_Normalize(axis);
    SinCosResult trig = SinCosF(turns);
    float c1 = 1.f - trig.cos;

    Mat4 res = Mat4_Diagonal(1.f);
    res.elem[0][0] = (axis.x * axis.x * c1) + trig.cos;
    res.elem[0][1] = (axis.x * axis.y * c1) + (axis.z * trig.sin);
    res.elem[0][2] = (axis.x * axis.z * c1) - (axis.y * trig.sin);

    res.elem[1][0] = (axis.y * axis.x * c1) - (axis.z * trig.sin);
    res.elem[1][1] = (axis.y * axis.y * c1) + trig.cos;
    res.elem[1][2] = (axis.y * axis.z * c1) + (axis.x * trig.sin);

    res.elem[2][0] = (axis.z * axis.x * c1) + (axis.y * trig.sin);
    res.elem[2][1] = (axis.z * axis.y * c1) - (axis.x * trig.sin);
    res.elem[2][2] = (axis.z * axis.z * c1) + trig.cos;
    return res;
}

static Mat4 Mat4_InvRotation(Mat4 mat)
{
    return Mat4_Transpose(mat);
}

static Mat4 Mat4_Scale(V3 scale)
{
    Mat4 res = Mat4_Diagonal(1.f);
    res.elem[0][0] = scale.x;
    res.elem[1][1] = scale.y;
    res.elem[2][2] = scale.z;
    return res;
}

static Mat4 Mat4_ScaleF(float scale)
{
    Mat4 res = Mat4_Diagonal(1.f);
    res.elem[0][0] = scale;
    res.elem[1][1] = scale;
    res.elem[2][2] = scale;
    return res;
}

static Mat4 Mat4_Translation(V3 move)
{
    Mat4 res = Mat4_Diagonal(1.f);
    res.elem[3][0] = move.x;
    res.elem[3][1] = move.y;
    res.elem[3][2] = move.z;
    return res;
}

static Mat4 Mat4_InvTranslation(Mat4 translation_mat)
{
    Mat4 res = translation_mat;
    res.elem[3][0] = -res.elem[3][0];
    res.elem[3][1] = -res.elem[3][1];
    res.elem[3][2] = -res.elem[3][2];
    return res;
}

static Mat4 Mat4_Perspective_RH_NO(float fov_y, float aspect_ratio, float near, float far)
{
    // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
    // Modified to work with +x fordward, -y right, +z up coordinate system (same as Source engine).
    float cotangent = 1.0f / TanF(fov_y * 0.5f);

    Mat4 res = {};
    res.elem[1][0] = -cotangent / aspect_ratio;

    res.elem[2][1] = cotangent;

    res.elem[0][2] = -(near + far) / (near - far);
    res.elem[3][2] = (2.0f * near * far) / (near - far);

    res.elem[0][3] = 1.0f;
    return res;
}

static inline Mat4 Mat4_InvPerspective_RH(Mat4 mat)
{
    Mat4 res = {};
    // Invert the perspective projection matrix
    float a = -1.0f / mat.elem[1][0]; // Corresponds to -aspect_ratio / cotangent
    float b = 1.0f / mat.elem[2][1];  // Corresponds to 1 / cotangent
    float c = mat.elem[0][2];         // -(near + far) / (near - far)
    float d = mat.elem[3][2];         // (2 * near * far) / (near - far)

    res.elem[1][0] = a;
    res.elem[2][1] = b;

    res.elem[3][0] = (1.0f / c) * -1.0f; // Inverting the Z mapping
    res.elem[0][1] = (1.0f / d);

    res.elem[2][3] = 1.0f; // Reverse the perspective divide
    return res;
}

// ---
// Quaternion
// ---
static Quat Quat_Add(Quat a, Quat b)
{
    return V4_Add(a, b);
}

static Quat Quat_Sub(Quat a, Quat b)
{
    return V4_Sub(a, b);
}

static Quat Quat_Mul(Quat a, Quat b)
{
    // @todo SIMD
    Quat res = {};
    res.x = b.w * +a.x;
    res.y = b.z * -a.x;
    res.z = b.y * +a.x;
    res.w = b.x * -a.x;

    res.x += b.z * +a.y;
    res.y += b.w * +a.y;
    res.z += b.x * -a.y;
    res.w += b.y * -a.y;

    res.x += b.y * -a.z;
    res.y += b.x * +a.z;
    res.z += b.w * +a.z;
    res.w += b.z * -a.z;

    res.x += b.x * +a.w;
    res.y += b.y * +a.w;
    res.z += b.z * +a.w;
    res.w += b.w * +a.w;
    return res;
}

static Quat Quat_Scale(Quat q, float scale)
{
    return V4_Scale(q, scale);
}

static float Quat_Inner(Quat a, Quat b)
{
    return V4_Inner(a, b);
}

static Quat Quat_Identity()
{
    return (Quat){0,0,0,1};
}

static Quat Quat_Inv(Quat q)
{
    float lensq = Quat_Inner(q, q);
    float inv_lensq = 1.f / lensq;
    Quat res =
    {
        -q.x,
        -q.y,
        -q.z,
        q.w,
    };
    return Quat_Scale(res, inv_lensq);
}

static Quat Quat_Normalize(Quat q)
{
    return V4_Normalize(q);
}

static Quat Quat_Mix(Quat a, Quat b, float wa, float wb)
{
    Quat res =
    {
        a.x*wa + b.x*wb,
        a.y*wa + b.y*wb,
        a.z*wa + b.z*wb,
        a.w*wa + b.w*wb,
    };
    return res;
}

static Quat Quat_NLerp(Quat a, Quat b, float t)
{
    return Quat_Mix(a, b, 1.f - t, t);
}

static Quat Quat_Lerp(Quat a, Quat b, float t)
{
    Quat res = Quat_NLerp(a, b, t);
    return Quat_Normalize(res);
}

static Quat Quat_SLerp(Quat a, Quat b, float t)
{
    float cos_theta = Quat_Inner(a, b);
    if (cos_theta < 0.f)
    {
        cos_theta = -cos_theta;
        b = Quat_Scale(b, -1.f);
    }

    if (cos_theta > 0.9995f)
    {
        // NOTE(lcf): Use Normalized Linear interpolation when vectors are roughly not L.I.
        return Quat_NLerp(a, b, t);
    }

    float angle = AcosF(cos_theta);
    float ta = SinF((1.f - t) * angle);
    float tb = SinF(t * angle);

    Quat res = Quat_Mix(a, b, ta, tb);
    return Quat_Normalize(res);
}

static Mat4 Mat4_Rotation_Quat(Quat q)
{
    Quat norm = Quat_Normalize(q);
    float xx = norm.x * norm.x;
    float yy = norm.y * norm.y;
    float zz = norm.z * norm.z;
    float xy = norm.x * norm.y;
    float xz = norm.x * norm.z;
    float yz = norm.y * norm.z;
    float wx = norm.w * norm.x;
    float wy = norm.w * norm.y;
    float wz = norm.w * norm.z;

    Mat4 res;
    res.elem[0][0] = 1.0f - 2.0f * (yy + zz);
    res.elem[0][1] = 2.0f * (xy + wz);
    res.elem[0][2] = 2.0f * (xz - wy);
    res.elem[0][3] = 0.0f;

    res.elem[1][0] = 2.0f * (xy - wz);
    res.elem[1][1] = 1.0f - 2.0f * (xx + zz);
    res.elem[1][2] = 2.0f * (yz + wx);
    res.elem[1][3] = 0.0f;

    res.elem[2][0] = 2.0f * (xz + wy);
    res.elem[2][1] = 2.0f * (yz - wx);
    res.elem[2][2] = 1.0f - 2.0f * (xx + yy);
    res.elem[2][3] = 0.0f;

    res.elem[3][0] = 0.0f;
    res.elem[3][1] = 0.0f;
    res.elem[3][2] = 0.0f;
    res.elem[3][3] = 1.0f;
    return res;
}

static Quat Quat_FromM4_RH(Mat4 mat)
{
    float t = 0.f;
    Quat q = {};

    if (mat.elem[2][2] < 0.0f)
    {
        if (mat.elem[0][0] > mat.elem[1][1])
        {
            t = 1 + mat.elem[0][0] - mat.elem[1][1] - mat.elem[2][2];
            q = (Quat)
            {
                t,
                mat.elem[0][1] + mat.elem[1][0],
                mat.elem[2][0] + mat.elem[0][2],
                mat.elem[1][2] - mat.elem[2][1]
            };
        }
        else
        {
            t = 1 - mat.elem[0][0] + mat.elem[1][1] - mat.elem[2][2];
            q = (Quat)
            {
                      mat.elem[0][1] + mat.elem[1][0],
                      t,
                      mat.elem[1][2] + mat.elem[2][1],
                      mat.elem[2][0] - mat.elem[0][2]
            };
        }
    }
    else
    {
        if (mat.elem[0][0] < -mat.elem[1][1])
        {
            t = 1 - mat.elem[0][0] - mat.elem[1][1] + mat.elem[2][2];
            q = (Quat)
            {
                      mat.elem[2][0] + mat.elem[0][2],
                      mat.elem[1][2] + mat.elem[2][1],
                      t,
                      mat.elem[0][1] - mat.elem[1][0]
            };
        }
        else
        {
            t = 1 + mat.elem[0][0] + mat.elem[1][1] + mat.elem[2][2];
            q = (Quat)
            {
                      mat.elem[1][2] - mat.elem[2][1],
                      mat.elem[2][0] - mat.elem[0][2],
                      mat.elem[0][1] - mat.elem[1][0],
                      t
            };
        }
    }

    q = Quat_Scale(q, 0.5f / SqrtF(t));
    return q;
}

static Quat Quat_FromAxisAngle_RH(V3 axis, float rot_turns)
{
    SinCosResult sc = SinCosF(rot_turns * 0.5f);
    axis = V3_Normalize(axis);
    axis = V3_Scale(axis, sc.sin);
    Quat res =
    {
        axis.x,
        axis.y,
        axis.z,
        sc.cos
    };
    return res;
}

static Quat Quat_FromNormalizedPair(V3 a, V3 b)
{
    V3 cross = V3_Cross(a, b);
    Quat res =
    {
        cross.x,
        cross.y,
        cross.z,
        1.f + V3_Inner(a, b),
    };
    return res;
}

static Quat Quat_FromPair(V3 a, V3 b)
{
    return Quat_FromNormalizedPair(V3_Normalize(a), V3_Normalize(b));
}

static V3 V3_Rotate(V3 v, Quat q)
{
    V3 q3 = {q.x, q.y, q.z};
    V3 t = V3_Scale(V3_Cross(q3, v), 2);
    return V3_Add(v, V3_Add(V3_Scale(t, q.w), V3_Cross(q3, t)));
}
