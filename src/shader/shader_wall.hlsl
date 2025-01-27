cbuffer UBO : register(b0, space1)
{
    float4x4 CameraMoveProj;
    float4x4 CameraRotationProj;
    float4x4 PerspectiveProj;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float3 UV : TEXCOORD3;
};

struct VSOutput
{
    float4 Color : TEXCOORD0;
    float3 UV : TEXCOORD1;
    float4 Position : SV_Position;
};

VSOutput ShaderWallVS(VSInput input)
{
    float3 world_sun_pos = normalize(float3(-0.5f, 0.5f, 1.f));
    float in_sun_coef = dot(world_sun_pos, input.Normal);
    float4 color = float4(input.Color, 1.0f);
    color.xyz *= clamp(in_sun_coef, 0.25f, 1.0f);

    float4x4 modelTransform = CameraMoveProj;
    modelTransform = mul(CameraRotationProj, modelTransform);
    modelTransform = mul(PerspectiveProj, modelTransform);

    VSOutput output;
    output.Color = color;
    output.Position = mul(modelTransform, float4(input.Position, 1.0f));
    output.UV = input.UV;
    return output;
}

Texture2DArray<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

float4 ShaderWallPS(VSOutput input) : SV_Target0
{
    float4 tex_color = Texture.Sample(Sampler, input.UV);
    return tex_color * input.Color;
}
