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

#define USES_INSTANCE_BUFFER (IS_RIGID || IS_SKINNED)

#include "shader_util.hlsl"

struct UniformData
{
  Mat4 camera_transform;
  Mat4 shadow_transform;
  V3 camera_position;
  V3 background_color;
  V3 towards_sun_dir;
};

cbuffer VertexUniformBuf : register(b0, space1) { UniformData UniV; };
cbuffer PixelUniformBuf  : register(b0, space3) { UniformData UniP; };

struct VSInput
{
  Quat normal_rot : TEXCOORD0;
  V3   position   : TEXCOORD1;

#if IS_RIGID
  uint color : TEXCOORD2;
#endif

#if IS_SKINNED
  uint color          : TEXCOORD2;
  uint joints_packed4 : TEXCOORD3;
  V4   weights        : TEXCOORD4;
#endif

#if IS_TEXTURED
  V2 uv      : TEXCOORD2;
  uint color : TEXCOORD3;
#endif

  uint InstanceIndex : SV_InstanceID;
};

struct VertexToFragment
{
  V4 color      : TEXCOORD0;
  V4 shadow_p   : TEXCOORD1; // position in shadow space
  V3 world_p    : TEXCOORD2;
#if IS_TEXTURED
  V2 uv           : TEXCOORD3;
  Mat3 normal_rot : TEXCOORD4;
#else
  Mat3 normal_rot : TEXCOORD3;
#endif
  V4 clip_p : SV_Position;
};

#if USES_INSTANCE_BUFFER
struct VSModelInstanceData
{
  float4x4 transform;
  uint color;
  uint pose_offset;
};
StructuredBuffer<VSModelInstanceData> InstanceBuf : register(t0);
#endif

#if IS_SKINNED
StructuredBuffer<float4x4> PoseBuf : register(t1);
#endif

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
static Quat Quat_Mul(Quat a, Quat b)
{
  Quat res;
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



VertexToFragment ShaderModelVS(VSInput input)
{
  float4 input_color = UnpackColor32(input.color);
  float4 color = input_color;

#if USES_INSTANCE_BUFFER
  VSModelInstanceData instance = InstanceBuf[input.InstanceIndex];
  float4 instance_color = UnpackColor32(instance.color);

  if (input.color == 0xff014b74) // @todo obviously temporary
    color = instance_color;
#endif



  // Position
#if USES_INSTANCE_BUFFER
  Mat4 position_transform = instance.transform;
#else
  Mat4 position_transform = Mat4_Identity();
#endif

#if IS_SKINNED
  {
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

    position_transform = mul(position_transform, pose_transform);
  }
#endif

  float4 world_p = mul(position_transform, float4(input.position, 1.0f));
  float4 clip_p = mul(UniV.camera_transform, world_p);

  Mat3 input_normal_mat = Mat3_Rotation_Quat(input.normal_rot);
  Mat3 position_rotation = Mat3_FromMat4(Mat4_RotationPart(position_transform));
  Mat3 normal_rotation = mul(position_rotation, input_normal_mat);

  // Return
  VertexToFragment frag;
  frag.color = color;
  frag.shadow_p = mul(UniV.shadow_transform, world_p);
  frag.world_p = world_p.xyz;
#if IS_TEXTURED
  frag.uv = input.uv;
#endif
  frag.normal_rot = normal_rotation;
  frag.clip_p = clip_p;
  return frag;
}

Texture2DArray<half> ShadowTexture : register(t0, space2);
SamplerState ShadowSampler : register(s0, space2);
#if IS_TEXTURED
Texture2DArray<float4> ColorTexture : register(t1, space2);
SamplerState ColorSampler : register(s1, space2);
#endif

float4 ShaderModelPS(VertexToFragment frag) : SV_Target0
{
  V4 color = frag.color;
  float shininess = 16.f;
  V3 face_normal = mul(frag.normal_rot, V3(0,0,1));

  // Load texture data
#if IS_TEXTURED
  V4 tex_color        = ColorTexture.Sample(ColorSampler, V3(frag.uv, 0.f));
  V4 tex_normal_og    = ColorTexture.Sample(ColorSampler, V3(frag.uv, 1.f));
  V4 tex_roughness    = ColorTexture.Sample(ColorSampler, V3(frag.uv, 2.f));
  //V4 tex_displacement = ColorTexture.Sample(ColorSampler, V3(frag.uv, 3.f));
  V4 tex_occlusion    = ColorTexture.Sample(ColorSampler, V3(frag.uv, 4.f));

  // apply color
  color *= tex_color;

  // swizzle normal components into engine format - ideally this would be done by asset preprocessor
  V3 tex_normal = tex_normal_og.yxz;
  tex_normal.y = 1.f - tex_normal.y;
  tex_normal = tex_normal*2.f - 1.f; // transform from [0, 1] to [-1; 1]

  V3 pixel_normal = normalize(mul(frag.normal_rot, tex_normal));

  // Apply shininess
  shininess = 32.f - 32.f*tex_roughness.x;
#else
  V3 pixel_normal = face_normal;
#endif

  // Shadow mapping
  float shadow = 0.f;
  {
    V3 shadow_proj = frag.shadow_p.xyz / frag.shadow_p.w; // this isn't needed for orthographic projection
    // [-1, 1] -> [0, 1]
    shadow_proj.x = shadow_proj.x * 0.5f + 0.5f;
    shadow_proj.y = shadow_proj.y * -0.5f + 0.5f;

    V3 shadow_dim = 0.f;
    ShadowTexture.GetDimensions(shadow_dim.x, shadow_dim.y, shadow_dim.z);
    V2 texel_size = 1.f / shadow_dim.xy;

    int sample_radius = 1;
    for (int x = -sample_radius; x <= sample_radius; x++)
    {
      for (int y = -sample_radius; y <= sample_radius; y++)
      {
        V2 shadow_coord = shadow_proj.xy + V2(x,y) * texel_size;
        float closest_depth = ShadowTexture.Sample(ShadowSampler, V3(shadow_coord, 0.f));

        // This hack is done because SDL_GPU doesn't support BORDER_COLOR for SDL_GPUSamplerAddressMode
        if (shadow_proj.x < 0.f || shadow_proj.x > 1.f || shadow_proj.y < 0.f || shadow_proj.y > 1.f)
          closest_depth = 1.f;

        float current_depth = shadow_proj.z;
        if (current_depth <= 1.f)
        {
          float bias = max(0.05f * (1.f - dot(UniP.towards_sun_dir, face_normal)), 0.005f);
          shadow += current_depth - bias > closest_depth ? 1.f : 0.f;
        }
      }
    }

    float sample_count = (sample_radius*2 + 1) * (sample_radius*2 + 1);
    shadow /= sample_count;
  }

  // Light
  float ambient = 0.2f;
  float specular = 0.0f;
  float diffuse = max(dot(UniP.towards_sun_dir, pixel_normal), 0.f);
  if (diffuse > 0.f)
  {
    V3 view_dir = normalize(UniP.camera_position - frag.world_p);
    V3 reflect_dir = reflect(-UniP.towards_sun_dir, pixel_normal);
    V3 halfway_dir = normalize(view_dir + UniP.towards_sun_dir);
    float specular_angle = max(dot(pixel_normal, halfway_dir), 0.f);
    specular = pow(specular_angle, shininess);
  }
  color.xyz *= ambient + (diffuse + specular) * (1.f - shadow);

  // Apply fog
  {
    float pixel_distance = distance(frag.world_p, UniP.camera_position);
    float fog_min = 1000.f;
    float fog_max = 2000.f;

    float fog_t = smoothstep(fog_min, fog_max, pixel_distance);
    color = lerp(color, V4(UniP.background_color, 1.f), fog_t);
  }
  return color;
}
