typedef float4 Quat;
typedef float4 V4;
typedef float3 V3;
typedef float2 V2;
typedef float4x4 Mat4;
typedef float3x3 Mat3;
typedef uint U32;
typedef int I32;

static V4 UnpackColor32(U32 packed)
{
  float inv = 1.f / 255.f;
  V4 res;
  res.r = (packed & 255) * inv;
  res.g = ((packed >> 8) & 255) * inv;
  res.b = ((packed >> 16) & 255) * inv;
  res.a = ((packed >> 24) & 255) * inv;
  return res;
}
