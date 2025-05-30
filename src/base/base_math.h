//
// Axis
//
typedef enum {
  Axis2_X,
  Axis2_Y,
  Axis2_COUNT
} Axis2;

static Axis2 Axis2_Other(Axis2 axis)
{
  return (axis == Axis2_X ? Axis2_Y : Axis2_X);
}

//
// Scalar math
//
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(min, max, val) (((val)<(min)) ? (min) : ((val)>(max))?(max):(val))

#define TURNS_TO_RAD (2.f*SDL_PI_F)
#define RAD_TO_TURNS (1.f/(2.f*SDL_PI_F))

static float FSquare(float a) { return a*a; }
static float FSqrt(float a) { return SDL_sqrtf(a); }
static float FInvSqrt(float a) { return 1.f / FSqrt(a); } // @todo use SSE intrinsic?
static float FAbs(float a) { return (a < 0.f ? -a : a); }
static float FSign(float a) { return (a < 0.f ? -1.f : 1.f); }
// @todo custom sincos that with turn input
static float FSin(float turns) { return SDL_sinf(turns*TURNS_TO_RAD); }
static float FCos(float turns) { return SDL_cosf(turns*TURNS_TO_RAD); }
static float FTan(float turns) { return SDL_tanf(turns*TURNS_TO_RAD); }
static float FAsin(float value) { return SDL_asinf(value)*RAD_TO_TURNS; }
static float FAcos(float value) { return SDL_acosf(value)*RAD_TO_TURNS; }

static float FCeil(float a) { return SDL_ceilf(a); }
static float FFloor(float a) { return SDL_floorf(a); }
static float FTruncate(float a) { return SDL_truncf(a); }
static float FRound(float a) { return SDL_roundf(a); } // there is also SDL_lroundf variant that rounds to nearest long

static float FLerp(float a, float b, float t) { return a + (b - a) * t; }
static float FModulo(float a, float mod) { return a - FTruncate(a / mod) * mod; }
static float FWrap(float min, float max, float a)
{
  float range = max - min;
  float offset = a - min;
  return (offset - (FFloor(offset / range) * range) + min);
}

// Integer scalar math
static I32 MostSignificantBitU32(U32 value) { return SDL_MostSignificantBitIndex32(value); }

static U32 U32_CeilPow2(U32 v)
{
  if (!v) return 0;
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

static void *AlignPointerUp(void *ptr, U64 alignment) {
  U64 pointer = (U64)ptr;
  pointer += (alignment - 1);
  pointer = pointer & (~((alignment - 1)));
  return (void *)pointer;
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
  return (SinCosResult){FSin(turns), FCos(turns)};
}

//
// Vectors
//
#include "gen_vector_math.h"

//
// Float vectors - additional functions
//
static V2 V2_Lerp(V2 a, V2 b, float t) { return (V2){FLerp(a.x, b.x, t), FLerp(a.y, b.y, t)}; }
static V3 V3_Lerp(V3 a, V3 b, float t) { return (V3){FLerp(a.x, b.x, t), FLerp(a.y, b.y, t), FLerp(a.z, b.z, t)}; }
static V4 V4_Lerp(V4 a, V4 b, float t) { return (V4){FLerp(a.x, b.x, t), FLerp(a.y, b.y, t), FLerp(a.z, b.z, t), FLerp(a.w, b.w, t)}; }

static float V2_LengthSq(V2 a) { return V2_Dot(a, a); }
static float V3_LengthSq(V3 a) { return V3_Dot(a, a); }
static float V4_LengthSq(V4 a) { return V4_Dot(a, a); }

static float V2_Length(V2 a) { return FSqrt(V2_LengthSq(a)); }
static float V3_Length(V3 a) { return FSqrt(V3_LengthSq(a)); }
static float V4_Length(V4 a) { return FSqrt(V4_LengthSq(a)); }

static V2 V2_Normalize(V2 a)
{
  float len_sq = V2_LengthSq(a);
  if (len_sq) {
    float len_inv = FInvSqrt(len_sq);
    a = V2_Scale(a, len_inv);
  }
  return a;
}
static V3 V3_Normalize(V3 a)
{
  float len_sq = V3_LengthSq(a);
  if (len_sq) {
    float len_inv = FInvSqrt(len_sq);
    a = V3_Scale(a, len_inv);
  }
  return a;
}
static V4 V4_Normalize(V4 a)
{
  float len_sq = V4_LengthSq(a);
  if (len_sq) {
    float len_inv = FInvSqrt(len_sq);
    a = V4_Scale(a, len_inv);
  }
  return a;
}

static V2 V2_NanToZero(V2 a)
{
  if (a.x != a.x) a.x = 0.f;
  if (a.y != a.y) a.y = 0.f;
  return a;
}
static V3 V3_NanToZero(V3 a)
{
  if (a.x != a.x) a.x = 0.f;
  if (a.y != a.y) a.y = 0.f;
  if (a.z != a.z) a.z = 0.f;
  return a;
}
static V4 V4_NanToZero(V4 a)
{
  if (a.x != a.x) a.x = 0.f;
  if (a.y != a.y) a.y = 0.f;
  if (a.z != a.z) a.z = 0.f;
  if (a.w != a.w) a.w = 0.f;
  return a;
}

// Axis
static V2 AxisV2_X() { return (V2){1,0}; }
static V2 AxisV2_Y() { return (V2){0,1}; }
//
static V3 AxisV3_X() { return (V3){1,0,0}; }
static V3 AxisV3_Y() { return (V3){0,1,0}; }
static V3 AxisV3_Z() { return (V3){0,0,1}; }
//
static V4 AxisV4_X() { return (V4){1,0,0,0}; }
static V4 AxisV4_Y() { return (V4){0,1,0,0}; }
static V4 AxisV4_Z() { return (V4){0,0,1,0}; }
static V4 AxisV4_W() { return (V4){0,0,0,1}; }

// Ctors V2
static V2 V2_FromV3_XY(V3 v) { return (V2){v.x, v.y}; }
static V2 V2_FromV2I16(V2I16 v) {return (V2){v.x, v.y}; }

// Ctors V3
static V3 V3_From_XY_Z(V2 xy, float z) { return (V3){xy.x, xy.y, z}; }
static V3 V3_From_XZ_Y(V2 xz, float y) { return (V3){xz.x, y, xz.y}; }
static V3 V3_From_YX_Z(V2 xy, float z) { return (V3){xy.y, xy.x, z}; }
static V3 V3_From_ZY_X(V2 zy, float x) { return (V3){zy.x, zy.y, x}; }
static V3 V3_FromV4_XYZ(V4 vec) { return (V3){vec.x, vec.y, vec.z}; }

// Ctors V4
static V4 V4_From_XYZ_W(V3 xyz, float w) { return (V4){xyz.x, xyz.y, xyz.z, w}; }

// Float vectors - one-off functions
static float FAtan2(V2 vec) { return SDL_atan2f(vec.x, vec.y)*RAD_TO_TURNS; }

static V2 V2_RotateClockwise90(V2 a)
{
  // rotation matrix
  // [ cos(-0.5pi) -sin(-0.5pi) ] [ x ]
  // [ sin(-0.5pi)  cos(-0.5pi) ] [ y ]
  float cos = 0;  // cos(-0.5pi)
  float sin = -1; // sin(-0.5pi)

  float x_prim = V2_Dot((V2){cos, -sin}, a);
  float y_prim = V2_Dot((V2){sin,  cos}, a);

  return (V2){x_prim, y_prim};
}
static V2 V2_RotateCounterclockwise90(V2 a)
{
  // rotation matrix
  // [ cos(0.5pi) -sin(0.5pi) ] [ x ]
  // [ sin(0.5pi)  cos(0.5pi) ] [ y ]
  float cos = 0; // cos(0.5pi)
  float sin = 1; // sin(0.5pi)

  float x_prim = V2_Dot((V2){cos, -sin}, a);
  float y_prim = V2_Dot((V2){sin,  cos}, a);

  return (V2){x_prim, y_prim};
}

static V2 V2_RotateSinCos(V2 a, SinCosResult sincos)
{
  float x_prim = V2_Dot((V2){sincos.cos, -sincos.sin}, a);
  float y_prim = V2_Dot((V2){sincos.sin, sincos.cos}, a);
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

// Quaternion definition
typedef V4 Quat;

//
// Matrix
// Heavy duty matrix math funcs were ported from HandmadeMath.h
//
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

static bool Mat4_IsIdentity(Mat4 mat)
{
  Mat4 id = Mat4_Identity();
  return Memeq(&id, &mat, sizeof(id));
}

static Mat4 Mat4_Transpose(Mat4 mat)
{
  // @todo SIMD
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

static Mat4 Mat4_InvScale(Mat4 scale_mat)
{
  Mat4 res = scale_mat;
  res.elem[0][0] = 1.f / scale_mat.elem[0][0];
  res.elem[1][1] = 1.f / scale_mat.elem[1][1];
  res.elem[2][2] = 1.f / scale_mat.elem[2][2];
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

static Mat4 Mat4_LeaveOnlyTranslation(Mat4 mat)
{
  Mat4 res = Mat4_Identity();
  res.elem[3][0] = mat.elem[3][0];
  res.elem[3][1] = mat.elem[3][1];
  res.elem[3][2] = mat.elem[3][2];
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

static Mat4 Mat4_Perspective(float fov_y, float aspect_ratio, float near, float far)
{
  // Modified to work with +x fordward, -y right, +z up coordinate system (same as Source engine).
  float cotangent = 1.0f / FTan(fov_y * 0.5f);
  Mat4 res = {};
  res.elem[0][2] = -far / (near - far); // X -> -Z
  res.elem[0][3] = 1.0f; // X -> W
  res.elem[1][0] = -cotangent / aspect_ratio; // -Y -> X
  res.elem[2][1] = cotangent; // Z -> Y
  res.elem[3][2] = (near * far) / (near - far); // W -> Z
  return res;
}

static Mat4 Mat4_Orthographic(float left, float right, float bottom, float top, float near, float far)
{
  Mat4 res = {};
  res.elem[0][2] = -1.f / (near - far); // X -> -Z
  res.elem[1][0] = -2.0f / (right - left); // -Y -> X
  res.elem[2][1] = 2.0f / (top - bottom); // Z -> Y
  res.elem[3][3] = 1.0f; // W -> W

  res.elem[3][0] = (left + right) / (left - right); // W -> X
  res.elem[3][1] = (bottom + top) / (bottom - top); // W -> Y
  res.elem[3][2] = near / (near - far); // W -> Z
  return res;
}


//
// Quaternions
//
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

static float Quat_Dot(Quat a, Quat b)
{
  return V4_Dot(a, b);
}

static Quat Quat_Identity()
{
  return (Quat){0,0,0,1};
}

static bool Quat_IsIdentity(Quat q)
{
  Quat i = Quat_Identity();
  return Memeq(&q, &i, sizeof(Quat));
}

static Quat Quat_Inv(Quat q)
{
  float lensq = Quat_Dot(q, q);
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
  float cos_theta = Quat_Dot(a, b);
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

  float angle = FAcos(cos_theta);
  float ta = FSin((1.f - t) * angle);
  float tb = FSin(t * angle);

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

  q = Quat_Scale(q, 0.5f / FSqrt(t));
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
    1.f + V3_Dot(a, b),
  };
  return res;
}

static Quat Quat_FromPair(V3 a, V3 b)
{
  return Quat_FromNormalizedPair(V3_Normalize(a), V3_Normalize(b));
}

static Quat Quat_FromZupCrossV3(V3 a)
{
  return Quat_FromNormalizedPair((V3){0,0,1}, V3_Normalize(a));
}

static V3 V3_Rotate(V3 v, Quat q)
{
  V3 q3 = {q.x, q.y, q.z};
  V3 t = V3_Scale(V3_Cross(q3, v), 2);
  return V3_Add(v, V3_Add(V3_Scale(t, q.w), V3_Cross(q3, t)));
}

//
// Ranges - @todo generate these
//
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

//
// Color
//
static U32 Color32_RGBAi(U32 r, U32 g, U32 b, U32 a)
{
  U32 res = ((a << 24) |
             (b << 16) |
             (g << 8) |
             r);
  return res;
}

static U32 Color32_RGBi(U32 r, U32 g, U32 b)
{
  return Color32_RGBAi(r, g, b, 255);
}

static U32 Color32_Grayi(U32 rgb)
{
  return Color32_RGBi(rgb, rgb, rgb);
}

static U32 Color32_RGBAf(float r, float g, float b, float a)
{
  U32 ri = (U32)(r * 255.f) & 255;
  U32 gi = (U32)(g * 255.f) & 255;
  U32 bi = (U32)(b * 255.f) & 255;
  U32 ai = (U32)(a * 255.f) & 255;
  return Color32_RGBAi(ri, gi, bi, ai);
}

static U32 Color32_RGBf(float r, float g, float b)
{
  U32 ri = (U32)(r * 255.f) & 255;
  U32 gi = (U32)(g * 255.f) & 255;
  U32 bi = (U32)(b * 255.f) & 255;
  return Color32_RGBAi(ri, gi, bi, 255);
}

static U32 Color32_Grayf(float rgb)
{
  return Color32_RGBf(rgb, rgb, rgb);
}

static U32 Color32_V3(V3 color)
{
  return Color32_RGBf(color.x, color.y, color.z);
}

static U32 Color32_V4(V4 color)
{
  return Color32_RGBAf(color.x, color.y, color.z, color.w);
}

static V4 V4_UnpackColor32(U32 packed)
{
  float inv = 1.f / 255.f;
  V4 res;
  res.x = (packed & 255) * inv;
  res.y = ((packed >> 8) & 255) * inv;
  res.z = ((packed >> 16) & 255) * inv;
  res.w = ((packed >> 24) & 255) * inv;
  return res;
}

static U32 Color32_Lerp(U32 c0, U32 c1, float t)
{
  V4 v0 = V4_UnpackColor32(c0);
  V4 v1 = V4_UnpackColor32(c1);
  V4 lerped = V4_Lerp(v0, v1, t);
  U32 res = Color32_V4(lerped);
  return res;
}
