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
#define World_DxShaderVS World_DxShaderRigidVS
#define World_DxShaderPS World_DxShaderRigidPS
#endif
#if IS_SKINNED
#define World_DxShaderVS World_DxShaderSkinnedVS
#define World_DxShaderPS World_DxShaderSkinnedPS
#endif
#if IS_TEXTURED
#define World_DxShaderVS World_DxShaderMeshVS
#define World_DxShaderPS World_DxShaderMeshPS
#endif

#define HAS_INSTANCE_BUFFER (IS_RIGID || IS_SKINNED)
#define HAS_COLOR (IS_RIGID || IS_SKINNED)

#include "shader_util.hlsl"

struct WORLD_DX_Uniform
{
  Mat4 camera_transform;
  Mat4 shadow_transform;
  V3 camera_position;
  V3 sun_dir;

  U32 fog_color; // RGBA
  U32 sky_ambient; // RGBA
  U32 sun_diffuse; // RGBA
  U32 sun_specular; // RGBA

  U32 material_diffuse; // RGBA
  U32 material_specular; // RGBA
  float material_shininess;
  float material_loaded_t;
};

cbuffer VertexUniformBuf : register(b0, space1) { WORLD_DX_Uniform UniV; };
cbuffer PixelUniformBuf  : register(b0, space3) { WORLD_DX_Uniform UniP; };

struct WORLD_DX_Vertex
{
  V3 normal : TEXCOORD0;
  V3 position : TEXCOORD1;

#if IS_SKINNED
  U32 joints_packed4 : TEXCOORD2;
  V4  weights        : TEXCOORD3;
#endif

#if IS_TEXTURED
  V2  uv    : TEXCOORD2;
#endif

  U32 instance_index : SV_InstanceID;
};

struct WORLD_DX_Fragment
{
  V4 shadow_p   : TEXCOORD1; // position in shadow space
  V3 world_p    : TEXCOORD2;
#if IS_TEXTURED
  V2 uv           : TEXCOORD3;
  Mat3 normal_rot : TEXCOORD4;
#else
  Mat3 normal_rot : TEXCOORD3;
#endif

  V4 vertex_p : SV_Position;
};

#if HAS_INSTANCE_BUFFER
struct WORLD_DX_Instance
{
  Mat4 transform;
  U32 color;
  U32 pose_offset; // in indices; unused for rigid
};
StructuredBuffer<WORLD_DX_Instance> InstanceBuf : register(t0);
#endif

#if IS_SKINNED
StructuredBuffer<Mat4> PoseBuf : register(t1);
#endif


WORLD_DX_Fragment World_DxShaderVS(WORLD_DX_Vertex input)
{
  // Position
#if HAS_INSTANCE_BUFFER
  WORLD_DX_Instance instance = InstanceBuf[input.instance_index];
  Mat4 position_transform = instance.transform;
#else
  Mat4 position_transform = Mat4_Identity();
#endif

#if IS_SKINNED
  {
    U32 joint0 = (input.joints_packed4      ) & 0xff;
    U32 joint1 = (input.joints_packed4 >>  8) & 0xff;
    U32 joint2 = (input.joints_packed4 >> 16) & 0xff;
    U32 joint3 = (input.joints_packed4 >> 24) & 0xff;
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

  V4 world_p = mul(position_transform, float4(input.position, 1.0f));
  V4 vertex_p = mul(UniV.camera_transform, world_p);

  Quat normal_rot = Quat_FromZupCrossV3(input.normal);
  //Quat normal_rot = Quat(0,0,0,1);
  Mat3 input_normal_mat = Mat3_Rotation_Quat(normal_rot);
  Mat3 position_rotation = Mat3_FromMat4(Mat4_RotationPart(position_transform));
  Mat3 normal_rotation = mul(position_rotation, input_normal_mat);

  // Return
  WORLD_DX_Fragment frag;
  frag.shadow_p = mul(UniV.shadow_transform, world_p);
  frag.world_p = world_p.xyz;
#if IS_TEXTURED
  frag.uv = input.uv;
#endif
  frag.normal_rot = normal_rotation;
  frag.vertex_p = vertex_p;
  return frag;
}

Texture2DArray<half> ShadowTexture : register(t0, space2);
SamplerState ShadowSampler : register(s0, space2);
#if IS_TEXTURED
Texture2DArray<float4> ColorTexture : register(t1, space2);
SamplerState ColorSampler : register(s1, space2);
#endif

V4 World_DxShaderPS(WORLD_DX_Fragment frag) : SV_Target0
{
  V3 fog_color = UnpackColor32(UniP.fog_color).xyz;
  V3 sky_ambient = UnpackColor32(UniP.sky_ambient).xyz;
  V4 sun_diffuse = UnpackColor32(UniP.sun_diffuse);
  V4 sun_specular = UnpackColor32(UniP.sun_specular);
  V3 material_diffuse = UnpackColor32(UniP.material_diffuse).xyz;
  V4 material_specular = UnpackColor32(UniP.material_specular);
  float material_shininess = UniP.material_shininess;

  V3 face_normal = mul(frag.normal_rot, V3(0,0,1));
  V3 pixel_normal = face_normal;

  // Load texture data
#if IS_TEXTURED
  V3 tex_color  = ColorTexture.Sample(ColorSampler, V3(frag.uv, 0.f)).xyz;
  V3 tex_normal = ColorTexture.Sample(ColorSampler, V3(frag.uv, 1.f)).xyz;
  float tex_roughness = ColorTexture.Sample(ColorSampler, V3(frag.uv, 2.f)).x;
  // @todo tex_occlusion
  // @todo tex_displacement

  // swizzle normal components into engine format - @todo do this in asset baker
  tex_normal.y = 1.f - tex_normal.y;
  tex_normal = tex_normal*2.f - 1.f; // transform from [0, 1] to [-1; 1]

  // Apply default values when texture wasn't loaded yet
  {
    float t = UniP.material_loaded_t;
    tex_color       = lerp(fog_color,    tex_color,     t);
    tex_normal      = lerp(pixel_normal, tex_normal,    t);
    tex_roughness   = lerp(0.5f,         tex_roughness, t);
    //tex_occlusion = lerp(0.5f,         tex_occlusion, t);
  }

  // Use data loaded from textures
  material_diffuse = tex_color;
  pixel_normal = normalize(mul(frag.normal_rot, tex_normal));
  material_shininess *= (1.f - tex_roughness);
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
          float bias = max(0.05f * (1.f - dot(-UniP.sun_dir, face_normal)), 0.005f);
          shadow += current_depth - bias > closest_depth ? 1.f : 0.f;
        }
      }
    }

    float sample_count = (sample_radius*2 + 1) * (sample_radius*2 + 1);
    shadow /= sample_count;
  }

  // Light
  float specular_factor = 0.0f;
  float diffuse_factor = max(dot(-UniP.sun_dir, pixel_normal), 0.f);
  if (diffuse_factor > 0.f)
  {
    V3 view_dir = normalize(UniP.camera_position - frag.world_p);
    V3 halfway_dir = normalize(view_dir + UniP.sun_dir); // @todo previously it was -UniP.sun_dir, was it a bug?
    float specular_angle = max(dot(pixel_normal, halfway_dir), 0.f);
    specular_factor = pow(specular_angle, material_shininess);
  }

  V3 color_ambient = sky_ambient * material_diffuse;
  V3 color_diffuse = diffuse_factor * sun_diffuse * material_diffuse;
  V3 color_specular = specular_factor * sun_specular * material_specular;
  V3 color = color_ambient + (color_diffuse + color_specular) * (1.f - shadow);

  // Apply fog
  {
    float pixel_distance = distance(frag.world_p, UniP.camera_position);
    float fog_min = 1000.f;
    float fog_max = 2000.f;

    float fog_t = smoothstep(fog_min, fog_max, pixel_distance);
    color = lerp(color, fog_color, fog_t);
  }

  return V4(color, 1.f);
}
