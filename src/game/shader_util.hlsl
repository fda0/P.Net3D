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

static Mat4 Mat4_Identity()
{
  return Mat4(1.0f, 0.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f, 0.0f,
              0.0f, 0.0f, 1.0f, 0.0f,
              0.0f, 0.0f, 0.0f, 1.0f);
}

static Mat4 Mat4_RotationPart(Mat4 mat)
{
  // Extract the 3x3 submatrix columns
  // @todo is this extraction correct? - check with non-uniform scaling
  V3 axis_x = normalize(V3(mat._m00, mat._m10, mat._m20));
  V3 axis_y = normalize(V3(mat._m01, mat._m11, mat._m21));
  V3 axis_z = normalize(V3(mat._m02, mat._m12, mat._m22));

  // Rebuild the matrix with normalized vectors and zero translation
  mat._m00 = axis_x.x; mat._m01 = axis_y.x; mat._m02 = axis_z.x; mat._m03 = 0.f;
  mat._m10 = axis_x.y; mat._m11 = axis_y.y; mat._m12 = axis_z.y; mat._m13 = 0.f;
  mat._m20 = axis_x.z; mat._m21 = axis_y.z; mat._m22 = axis_z.z; mat._m23 = 0.f;
  mat._m30 = 0.f;      mat._m31 = 0.f;      mat._m32 = 0.f;      mat._m33 = 1.f;
  return mat;
}

static Mat3 Mat3_FromMat4(float4x4 mat)
{
  Mat3 res;
  res._m00 = mat._m00;
  res._m01 = mat._m01;
  res._m02 = mat._m02;
  res._m10 = mat._m10;
  res._m11 = mat._m11;
  res._m12 = mat._m12;
  res._m20 = mat._m20;
  res._m21 = mat._m21;
  res._m22 = mat._m22;
  return res;
}

static Mat3 Mat3_Rotation_Quat(Quat q)
{
  Quat norm = normalize(q);
  float xx = norm.x * norm.x;
  float yy = norm.y * norm.y;
  float zz = norm.z * norm.z;
  float xy = norm.x * norm.y;
  float xz = norm.x * norm.z;
  float yz = norm.y * norm.z;
  float wx = norm.w * norm.x;
  float wy = norm.w * norm.y;
  float wz = norm.w * norm.z;

  Mat3 res;
  res._m00 = 1.0f - 2.0f * (yy + zz);
  res._m10 = 2.0f * (xy + wz);
  res._m20 = 2.0f * (xz - wy);

  res._m01 = 2.0f * (xy - wz);
  res._m11 = 1.0f - 2.0f * (xx + zz);
  res._m21 = 2.0f * (yz + wx);

  res._m02 = 2.0f * (xz + wy);
  res._m12 = 2.0f * (yz - wx);
  res._m22 = 1.0f - 2.0f * (xx + yy);
  return res;
}
