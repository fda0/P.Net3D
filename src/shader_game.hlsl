cbuffer UBO : register(b0, space1)
{
    float4x4 ModelViewProj;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Color : TEXCOORD1;
};

struct VSOutput
{
    float4 Color : TEXCOORD0;
    float4 Position : SV_Position;
};

VSOutput ShaderGameVS(VSInput input)
{
    VSOutput output;
    output.Color = float4(input.Color, 1.0f);
    output.Position = mul(ModelViewProj, float4(input.Position, 1.0f));
    return output;
}

float4 ShaderGamePS(VSOutput input) : SV_Target0
{
    return input.Color;
}
