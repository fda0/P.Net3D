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

cbuffer UBO : register(b0, space1)
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
};

StructuredBuffer<VSModelInstanceData> InstanceBuffer : register(t0);

float4x4 Mat4_RotationPart(float4x4 mat)
{
  // @todo in the future this matrix will need to be normalized
  // to eliminate scaling from the matrix
  mat._m03 = 0.f;
  mat._m13 = 0.f;
  mat._m23 = 0.f;
  mat._m33 = 1.f;
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
  VSModelInstanceData instance = InstanceBuffer[input.InstanceIndex];

  float3 normal = mul(Mat4_RotationPart(instance.transform), float4(input.normal, 1.f)).xyz;
  float3 world_sun_pos = normalize(float3(-0.5f, 0.5f, 1.f));
  float in_sun_coef = dot(world_sun_pos, normal);

  float4 color = float4(input.color, 1.0f);
  color *= instance.color;
  color.xyz *= clamp(in_sun_coef, 0.25f, 1.0f);

  float4x4 model_transform = mul(camera_transform, instance.transform);
  float4 position = float4(input.position, 1.0f);

#if IS_SKINNED
#  if 0
  uint joint0 = (input.joints_packed4      ) & 0xff;
  uint joint1 = (input.joints_packed4 >>  8) & 0xff;
  uint joint2 = (input.joints_packed4 >> 16) & 0xff;
  uint joint3 = (input.joints_packed4 >> 24) & 0xff;
  //joint0 = joint1 = joint2 = joint3 = 1;

  float4x4 joint_mat0 = joint_inv_bind_mats[joint0];
  float4x4 joint_mat1 = joint_inv_bind_mats[joint1];
  float4x4 joint_mat2 = joint_inv_bind_mats[joint2];
  float4x4 joint_mat3 = joint_inv_bind_mats[joint3];

  float4 pos0 = mul(position, joint_mat0);
  float4 pos1 = mul(position, joint_mat1);
  float4 pos2 = mul(position, joint_mat2);
  float4 pos3 = mul(position, joint_mat3);

  position =
    pos0 * input.weights.x +
    pos1 * input.weights.y +
    pos2 * input.weights.z +
    pos3 * input.weights.w;
#  endif
  position = mul(Mat4_Scale(40.f), position);
  position = mul(model_transform, position);

#else
  position.z -= 200.f;
  position = mul(model_transform, position);
#endif

  VSOutput output;
  output.color = color;
  output.position = position;
  return output;
}

float4 ShaderModelPS(VSOutput input) : SV_Target0
{
  return input.color;
}
