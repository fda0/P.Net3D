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
    uint InstanceIndex : SV_InstanceID;
};

struct VSOutput
{
    float4 Color : TEXCOORD0;
    float4 Position : SV_Position;
};

struct VSModelInstanceData
{
    float4x4 transform;
    float4 color;
};

StructuredBuffer<VSModelInstanceData> InstanceBuffer : register(t0);

VSOutput ShaderModelVS(VSInput input)
{
    VSModelInstanceData instance_data = InstanceBuffer[input.InstanceIndex];

    float3 world_sun_pos = normalize(float3(-0.5f, 0.5f, 1.f));
    float in_sun_coef = dot(world_sun_pos, input.Normal);
    float4 color = float4(input.Color, 1.0f);
    color *= instance_data.color;
    color.xyz *= clamp(in_sun_coef, 0.25f, 1.0f);

    float4x4 modelTransform = instance_data.transform;
    modelTransform = mul(CameraMoveProj, modelTransform);
    modelTransform = mul(CameraRotationProj, modelTransform);
    modelTransform = mul(PerspectiveProj, modelTransform);

    VSOutput output;
    output.Color = color;
    output.Position = mul(modelTransform, float4(input.Position, 1.0f));
    return output;
}

float4 ShaderModelPS(VSOutput input) : SV_Target0
{
    return input.Color;
}
