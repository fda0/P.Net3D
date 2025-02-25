cbuffer UBO : register(b0, space1)
{
  float4x4 CameraTransform;
};

struct VSInput
{
  float3 position : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float3 uv : TEXCOORD2;
  uint color : TEXCOORD3;
};

struct VSOutput
{
  float4 color : TEXCOORD0;
  float3 uv : TEXCOORD1;
  float4 position : SV_Position;
};

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

VSOutput ShaderWallVS(VSInput input)
{
  float3 world_sun_pos = normalize(float3(-0.4f, 0.5f, 1.f));
  float in_sun_coef = dot(world_sun_pos, input.normal);
  float4 color = UnpackColor32(input.color);
  color.xyz *= clamp(in_sun_coef, 0.2f, 1.0f);

  float4x4 model_transform = CameraTransform;

  VSOutput output;
  output.color = color;
  output.position = mul(model_transform, float4(input.position, 1.0f));
  output.uv = input.uv;
  return output;
}

Texture2DArray<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

float4 ShaderWallPS(VSOutput input) : SV_Target0
{
  float4 tex_color = Texture.Sample(Sampler, input.uv);
  float4 color = tex_color * input.color;
  return color;
}
