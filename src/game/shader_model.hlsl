#ifndef IS_RIGID
  #define IS_RIGID 0
#endif
#ifndef IS_SKINNED
  #define IS_SKINNED 0
#endif
#if IS_RIGID == IS_SKINNED
  #error "IS_RIGID is equal to IS_SKINNED"
#endif

#if IS_RIGID
#define ShaderModelVS ShaderRigidVS
#define ShaderModelPS ShaderRigidPS
#endif
#if IS_SKINNED
#define ShaderModelVS ShaderSkinnedVS
#define ShaderModelPS ShaderSkinnedPS
#endif

cbuffer UniformBuf : register(b0, space1)
{
  float4x4 camera_transform;
};

struct VSInput
{
  float3 position : TEXCOORD0;
  float3 color    : TEXCOORD1;
  float3 normal   : TEXCOORD2;
#if IS_SKINNED
  uint joints_packed4 : TEXCOORD3;
  float4 weights      : TEXCOORD4;
#endif
  uint InstanceIndex : SV_InstanceID;
};

struct VSOutput
{
  float4 color : TEXCOORD0;
  float4 position : SV_Position;
};

struct VSModelInstanceData
{
  float4x4 transform;
  float4 color;
#if IS_SKINNED
  uint pose_offset;
#endif
};

StructuredBuffer<VSModelInstanceData> InstanceBuf : register(t0);
#if IS_SKINNED
StructuredBuffer<float4x4> PoseBuf : register(t1);
#endif

float4x4 Mat4_RotationPart(float4x4 mat)
{
  // Extract the 3x3 submatrix columns
  // @todo is this extraction correct? - check with non-uniform scaling
  float3 axis_x = float3(mat._m00, mat._m10, mat._m20);
  float3 axis_y = float3(mat._m01, mat._m11, mat._m21);
  float3 axis_z = float3(mat._m02, mat._m12, mat._m22);

  // Normalize each axis to remove scaling
  float len_x = length(axis_x);
  if (len_x > 0.0001f) axis_x = axis_x / len_x;

  float len_y = length(axis_y);
  if (len_y > 0.0001f) axis_y = axis_y / len_y;

  float len_z = length(axis_z);
  if (len_z > 0.0001f) axis_z = axis_z / len_z;

  // Rebuild the matrix with normalized vectors and zero translation
  mat._m00 = axis_x.x; mat._m01 = axis_y.x; mat._m02 = axis_z.x; mat._m03 = 0.f;
  mat._m10 = axis_x.y; mat._m11 = axis_y.y; mat._m12 = axis_z.y; mat._m13 = 0.f;
  mat._m20 = axis_x.z; mat._m21 = axis_y.z; mat._m22 = axis_z.z; mat._m23 = 0.f;
  mat._m30 = 0.f;      mat._m31 = 0.f;      mat._m32 = 0.f;      mat._m33 = 1.f;
  return mat;
}

float4x4 Mat4_Identity()
{
  return float4x4(1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f);
}

float4x4 Mat4_TranslationPart(float4x4 mat)
{
  float4x4 res = Mat4_Identity();
  res._m03 = mat._m03;
  res._m13 = mat._m13;
  res._m23 = mat._m23;
  return res;
}

float4x4 Mat4_Scale(float scale)
{
  float4x4 res = Mat4_Identity();
  res._11 = scale;
  res._22 = scale;
  res._33 = scale;
  return res;
}

VSOutput ShaderModelVS(VSInput input)
{
  VSModelInstanceData instance = InstanceBuf[input.InstanceIndex];

  float4 position = float4(input.position, 1.0f);

#if IS_SKINNED
  uint joint0 = (input.joints_packed4      ) & 0xff;
  uint joint1 = (input.joints_packed4 >>  8) & 0xff;
  uint joint2 = (input.joints_packed4 >> 16) & 0xff;
  uint joint3 = (input.joints_packed4 >> 24) & 0xff;
  joint0 += instance.pose_offset;
  joint1 += instance.pose_offset;
  joint2 += instance.pose_offset;
  joint3 += instance.pose_offset;

  float4x4 pose_transform0 = PoseBuf[joint0];
  float4x4 pose_transform1 = PoseBuf[joint1];
  float4x4 pose_transform2 = PoseBuf[joint2];
  float4x4 pose_transform3 = PoseBuf[joint3];

  float4x4 pose_transform =
    pose_transform0 * input.weights.x +
    pose_transform1 * input.weights.y +
    pose_transform2 * input.weights.z +
    pose_transform3 * input.weights.w;

  float4x4 position_transform = mul(instance.transform, pose_transform);
  //position_transform = mul(Mat4_RotationPart(position_transform), position_transform);
#else
  float4x4 position_transform = instance.transform;
#endif

  position = mul(position_transform, position);
  position = mul(camera_transform, position);

  float3 normal = mul(Mat4_RotationPart(position_transform), float4(input.normal, 1.f)).xyz;
  float3 world_sun_pos = normalize(float3(-0.4f, 0.5f, 1.f));
  float in_sun_coef = dot(world_sun_pos, normal);

  float4 color = float4(input.color, 1.0f);
  color *= instance.color;
  color.xyz *= clamp(in_sun_coef, 0.2f, 1.0f);

  VSOutput output;
  output.color = color;
  output.position = position;
  return output;
}

float4 ShaderModelPS(VSOutput input) : SV_Target0
{
  return input.color;
}
