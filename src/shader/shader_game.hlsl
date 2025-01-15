cbuffer UBO : register(b0, space1)
{
    float4x4 ModelViewProj;
};

struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Color : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    uint InstanceIndex : SV_InstanceID;
};

struct VSOutput
{
    float4 Color : TEXCOORD0;
    float4 Normal : TEXCOORD1;
    float4 Position : SV_Position;
};

struct VSModelInstanceData
{
    float4x4 transform;
};

StructuredBuffer<VSModelInstanceData> InstanceBuffer : register(t0);

VSOutput ShaderGameVS(VSInput input)
{
    float4x4 rotation_mat = ModelViewProj;
    rotation_mat[3][0] = 0.f;
    rotation_mat[3][1] = 0.f;
    rotation_mat[3][2] = 0.f;
    rotation_mat[3][3] = 0.f;
    rotation_mat[0][3] = 0.f;
    rotation_mat[1][3] = 0.f;
    rotation_mat[2][3] = 0.f;
    rotation_mat[3][3] = 0.f;

    VSModelInstanceData instance_data = InstanceBuffer[input.InstanceIndex];

    float3 pos = input.Position;
    pos.x += 3.f * float(input.InstanceIndex);
    pos.z += 3.f * float(input.InstanceIndex);

    float4 color = float4(input.Color, 1.0f);
    color.x = instance_data.transform[0][0];
    color.z = instance_data.transform[1][1];

    float4x4 modelTransform = instance_data.transform;
    modelTransform = mul(modelTransform, ModelViewProj);

    VSOutput output;
    output.Color = color;
    output.Normal = mul(rotation_mat, float4(input.Normal, 1.0f));
    output.Position = mul(modelTransform, float4(pos, 1.0f));
    return output;
}

float4 ShaderGamePS(VSOutput input) : SV_Target0
{
    float3 sun_dir = normalize(float3(1.0f, 0.5f, -0.25f));
    float3 sun_proj = dot(sun_dir, input.Normal.xyz);

    float4 result = input.Color;
    result.xyz = lerp(result.xyz, sun_proj, 0.5f);
    return result;
}
