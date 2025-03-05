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

struct UniformData
{
  float4x4 CameraTransform;
  float3 CameraPosition;
  float3 BackgroundColor;
};

cbuffer VertexUniformBuf : register(b0, space1) { UniformData UniV; };
cbuffer PixelUniformBuf  : register(b0, space3) { UniformData UniP; };

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

struct VertexToFragment
{
  float4 color      : TEXCOORD0;
  float3 world_p    : TEXCOORD1;
  float3 normal     : TEXCOORD2;
#if IS_TEXTURED
  float3 uv         : TEXCOORD3;
  float3x3 rotation : TEXCOORD4;
#else
  float3x3 rotation : TEXCOORD3;
#endif
  float4 position   : SV_Position;
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

static float4x4 Mat4_Identity()
{
  return float4x4(1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f);
}

static float4x4 Mat4_RotationPart(float4x4 mat)
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

static float4x4 Mat4_TranslationPart(float4x4 mat)
{
  float4x4 res = Mat4_Identity();
  res._m03 = mat._m03;
  res._m13 = mat._m13;
  res._m23 = mat._m23;
  return res;
}

static float4x4 Mat4_Scale(float scale)
{
  float4x4 res = Mat4_Identity();
  res._11 = scale;
  res._22 = scale;
  res._33 = scale;
  return res;
}

static float3x3 Mat3_FromMat4(float4x4 mat)
{
  float3x3 res;
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

static float4 UnpackColor32(uint packed)
{
  float inv = 1.f / 255.f;
  float4 res;
  res.r = (packed & 255) * inv;
  res.g = ((packed >> 8) & 255) * inv;
  res.b = ((packed >> 16) & 255) * inv;
  res.a = ((packed >> 24) & 255) * inv;
  return res;
}

static float3 TowardsSunDir()
{
  return normalize(float3(-0.5f, 0.25f, 1.f));
}

typedef float4 Quat;
typedef float3 V3;
typedef float3x3 Mat3;

static Quat Quat_FromNormalizedPair(V3 a, V3 b)
{
  V3 cross_product = cross(a, b);
  Quat res =
  {
    cross_product.x,
    cross_product.y,
    cross_product.z,
    1.f + dot(a, b),
  };
  return res;
}
static Quat Quat_FromPair(V3 a, V3 b)
{
  return Quat_FromNormalizedPair(normalize(a), normalize(b));
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



VertexToFragment ShaderModelVS(VSInput input)
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
  position = mul(UniV.CameraTransform, position);

  // Color
  float4 color = input_color;
  if (input.color == 0xff014b74) // @todo obviously temporary solution
    color = instance_color;

  Mat3 input_normal_rot = Mat3_Rotation_Quat(Quat_FromNormalizedPair(float3(0,0,1), input.normal));
  Mat3 position_rotation = Mat3_FromMat4(Mat4_RotationPart(position_transform));
  float3 normal = mul(position_rotation, input.normal);
  Mat3 normal_rotation = mul(input_normal_rot, position_rotation);

  // Return
  VertexToFragment frag;
  frag.color = color;
  frag.world_p = world_p;
  frag.position = position;
  frag.normal = normal;
  frag.rotation = normal_rotation;
#if IS_TEXTURED
  frag.uv = input.uv;
#endif
  return frag;
}

#if IS_TEXTURED
Texture2DArray<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);
#endif

float4 ShaderModelPS(VertexToFragment frag) : SV_Target0
{
  float3 towards_light_dir = TowardsSunDir();
  float4 color = frag.color;
  float3 normal = frag.normal;
  float shininess = 16.f;

#if IS_TEXTURED
  float2 tex_uv = frag.uv.xy;

  // load tex data
  float4 tex_color        = Texture.Sample(Sampler, float3(tex_uv, 0.f));
  float4 tex_normal_og    = Texture.Sample(Sampler, float3(tex_uv, 1.f));
  float4 tex_roughness    = Texture.Sample(Sampler, float3(tex_uv, 2.f));
  //float4 tex_displacement = Texture.Sample(Sampler, float3(tex_uv, 3.f));
  float4 tex_occlusion    = Texture.Sample(Sampler, float3(tex_uv, 4.f));

  // apply color
  color *= tex_color;

  // swizzle normal components into engine format - ideally this would be done by asset preprocessor
  float4 tex_normal = tex_normal_og;
  tex_normal.x = 1.f - tex_normal_og.y;
  tex_normal.y = tex_normal_og.x;

  // transform normal
  tex_normal = tex_normal*2.f - 1.f; // transform from [0, 1] to [-1; 1]
  normal = mul(frag.rotation, tex_normal.xyz);

  // apply shininess
  shininess = 64.f - 64.f*tex_roughness.x;
#endif

  float ambient = 0.2f;
  float specular = 0.0f;
  float diffuse = max(dot(towards_light_dir, normal), 0.f);
  if (diffuse > 0.f)
  {
    float3 view_dir = normalize(UniP.CameraPosition - frag.world_p);
    float3 reflect_dir = reflect(-towards_light_dir, normal);
    float3 halfway_dir = normalize(view_dir + towards_light_dir);
    float specular_angle = max(dot(normal, halfway_dir), 0.f);
    specular = pow(specular_angle, shininess);

    // @todo multiple things about specular light seems bugged
    // 1. is camera_p one frame delayed?
    // 2. calculations/directions seem off, I need to play with simple programmer reference test assets
    // 3. shiny point seems offseted? idk! maybe it's the lack of ambient lighting
  }
  color.xyz *= ambient + diffuse + specular;

  // apply fog
  {
    float pixel_distance = distance(frag.world_p, UniP.CameraPosition);
    float fog_min = 800.f;
    float fog_max = 1000.f;

    float fog_t = smoothstep(fog_min, fog_max, pixel_distance);
    color = lerp(color, float4(UniP.BackgroundColor, 1.f), fog_t);
  }

  return color;
}
