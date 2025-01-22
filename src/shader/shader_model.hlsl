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
    float4 Normal : TEXCOORD1;
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
    float4x4 shadow_mat = CameraRotationProj;
    shadow_mat[3][0] = 0.f;
    shadow_mat[3][1] = 0.f;
    shadow_mat[3][2] = 0.f;
    shadow_mat[3][3] = 0.f;
    shadow_mat[0][3] = 0.f;
    shadow_mat[1][3] = 0.f;
    shadow_mat[2][3] = 0.f;
    shadow_mat[3][3] = 0.f;

    VSModelInstanceData instance_data = InstanceBuffer[input.InstanceIndex];

    float3 pos = input.Position;

    float4 color = float4(input.Color, 1.0f);
    color = color * instance_data.color;

    float4x4 idMat;
    idMat[0][0] = 1.f;
    idMat[0][1] = 0.f;
    idMat[0][2] = 0.f;
    idMat[0][3] = 0.f;
    idMat[1][0] = 0.f;
    idMat[1][1] = 1.f;
    idMat[1][2] = 0.f;
    idMat[1][3] = 0.f;
    idMat[2][0] = 0.f;
    idMat[2][1] = 0.f;
    idMat[2][2] = 1.f;
    idMat[2][3] = 0.f;
    idMat[3][0] = 0.f;
    idMat[3][1] = 0.f;
    idMat[3][2] = 0.f;
    idMat[3][3] = 1.f;

    float4x4 modelTransform = idMat;
    modelTransform = mul(instance_data.transform, modelTransform);
    modelTransform = mul(CameraMoveProj, modelTransform);
    modelTransform = mul(CameraRotationProj, modelTransform);
    modelTransform = mul(PerspectiveProj, modelTransform);

    VSOutput output;
    output.Color = color;
    output.Normal = mul(shadow_mat, float4(input.Normal, 1.0f));
    output.Position = mul(modelTransform, float4(pos, 1.0f));
    return output;
}

float4 ShaderModelPS(VSOutput input) : SV_Target0
{
    float3 sun_dir = normalize(float3(-1.0f, 0.5f, 0.25f));
    float3 sun_proj = dot(sun_dir, input.Normal.xyz);

    float4 result = input.Color;
    result.xyz = lerp(result.xyz, sun_proj, 0.5f);
    return result;
}
