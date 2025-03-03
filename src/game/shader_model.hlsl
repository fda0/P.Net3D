#ifndef IS_RIGID
  #define IS_RIGID 0
#endif
#ifndef IS_SKINNED
  #define IS_SKINNED 0
#endif
#ifndef IS_TEXTURED
  #define IS_TEXTURED 0
#endif
#if (IS_RIGID + IS_SKINNED + IS_TEXTURED) != 1
  #error "(IS_RIGID + IS_SKINNED + IS_TEXTURED) has to be equal to 1"
#endif

#if IS_RIGID
#define ShaderModelVS ShaderRigidVS
#define ShaderModelPS ShaderRigidPS
#endif
#if IS_SKINNED
#define ShaderModelVS ShaderSkinnedVS
#define ShaderModelPS ShaderSkinnedPS
#endif
#if IS_TEXTURED
#define ShaderModelVS ShaderWallVS
#define ShaderModelPS ShaderWallPS
#endif

cbuffer UniformBuf : register(b0, space1)
{
  float4x4 CameraTransform;
};

struct VSInput
{
#if IS_RIGID
  float3 position : TEXCOORD0;
  float3 normal   : TEXCOORD1;
  uint color      : TEXCOORD2;
#endif

#if IS_SKINNED
  float3 position     : TEXCOORD0;
  float3 normal       : TEXCOORD1;
  uint color          : TEXCOORD2;
  uint joints_packed4 : TEXCOORD3;
  float4 weights      : TEXCOORD4;
#endif

#if IS_TEXTURED
  float3 position : TEXCOORD0;
  float3 normal   : TEXCOORD1;
  float3 uv       : TEXCOORD2;
  uint color      : TEXCOORD3;
#endif

  uint InstanceIndex : SV_InstanceID;
};

struct VSOutput
{
  float4 color    : TEXCOORD0;
  float3 world_p  : TEXCOORD1;
  float3 normal   : TEXCOORD2;
  float3 camera_p : TEXCOORD3;

#if IS_TEXTURED
  float3 uv       : TEXCOORD4;
#endif

  float4 position : SV_Position;
};

struct VSModelInstanceData
{
  float4x4 transform;
  uint color;

#if IS_SKINNED
  uint pose_offset;
#endif
};

StructuredBuffer<VSModelInstanceData> InstanceBuf : register(t0);
#if IS_SKINNED
StructuredBuffer<float4x4> PoseBuf : register(t1);
#endif

float4x4 Mat4_Identity()
{
  return float4x4(1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f);
}

float4x4 Mat4_RotationPart(float4x4 mat)
{
  // Extract the 3x3 submatrix columns
  // @todo is this extraction correct? - check with non-uniform scaling
  float3 axis_x = normalize(float3(mat._m00, mat._m10, mat._m20));
  float3 axis_y = normalize(float3(mat._m01, mat._m11, mat._m21));
  float3 axis_z = normalize(float3(mat._m02, mat._m12, mat._m22));

  // Rebuild the matrix with normalized vectors and zero translation
  mat._m00 = axis_x.x; mat._m01 = axis_y.x; mat._m02 = axis_z.x; mat._m03 = 0.f;
  mat._m10 = axis_x.y; mat._m11 = axis_y.y; mat._m12 = axis_z.y; mat._m13 = 0.f;
  mat._m20 = axis_x.z; mat._m21 = axis_y.z; mat._m22 = axis_z.z; mat._m23 = 0.f;
  mat._m30 = 0.f;      mat._m31 = 0.f;      mat._m32 = 0.f;      mat._m33 = 1.f;
  return mat;
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

float4 UnpackColor32(uint packed)
{
  float inv = 1.f / 255.f;
  float4 res;
  res.r = (packed & 255) * inv;
  res.g = ((packed >> 8) & 255) * inv;
  res.b = ((packed >> 16) & 255) * inv;
  res.a = ((packed >> 24) & 255) * inv;
  return res;
}

static float3 SunDir()
{
  return normalize(float3(-0.4f, 0.5f, 1.f));
}

VSOutput ShaderModelVS(VSInput input)
{
  VSModelInstanceData instance = InstanceBuf[input.InstanceIndex];
  float4 input_color = UnpackColor32(input.color);
  float4 instance_color = UnpackColor32(instance.color);

  // Position
  float4x4 position_transform;
  {
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

    position_transform = mul(instance.transform, pose_transform);
#else
    position_transform = instance.transform;
#endif
  }

  float4 position = float4(input.position, 1.0f);
  position = mul(position_transform, position);
  float3 world_p = position.xyz;
  position = mul(CameraTransform, position);

  // Color
  float4 color = input_color;
  if (input.color == 0xff014b74) // @todo obviously temporary solution
    color = instance_color;

  float3 normal = mul(Mat4_RotationPart(position_transform), float4(input.normal, 1.f)).xyz;

  // @todo get from unifrom directly
  float3 camera_p;
  camera_p.x = CameraTransform._m03;
  camera_p.y = CameraTransform._m13;
  camera_p.z = CameraTransform._m23;

  // Return
  VSOutput output;
  output.color = color;
  output.world_p = world_p;
  output.position = position;
  output.normal = normal;
  output.camera_p = camera_p;
#if IS_TEXTURED
  output.uv = input.uv;
#endif
  return output;
}

#if IS_TEXTURED
Texture2DArray<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);
#endif

float4 ShaderModelPS(VSOutput input) : SV_Target0
{
  float3 sun_dir = SunDir();
  float4 color = input.color;

  float ambient = 0.2f;
  float specular = 0.0f;
  float diffuse = max(dot(sun_dir, input.normal), 0.f);
  if (diffuse > 0.f)
  {
    float shininess = 16.f;

    float3 view_dir = normalize(input.camera_p - input.world_p);
    float3 reflect_dir = reflect(-sun_dir, input.normal);
    float3 halfway_dir = normalize(view_dir + sun_dir);
    specular = pow(max(dot(input.normal, halfway_dir), 0.f), shininess);
  }
  color.xyz *= ambient + diffuse + specular;

#if IS_TEXTURED
  float4 tex_color = Texture.Sample(Sampler, input.uv);
  color *= tex_color;
#endif

  return color;
}
