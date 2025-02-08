cbuffer UBO : register(b0, space1)
{
    float4x4 CameraTransform;
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 color : TEXCOORD1;
    float3 normal : TEXCOORD2;
    uint InstanceIndex : SV_InstanceID;
};

struct VSOutput
{
    float4 color : TEXCOORD0;
    float4 position : SV_Position;
};

struct VSModelInstanceData
{
    float4x4 transform;
    float4 color;
};

StructuredBuffer<VSModelInstanceData> InstanceBuffer : register(t0);

float4x4 Mat4_RotationPart(float4x4 mat)
{
    // @todo in the future this matrix will need to be normalized
    // to eliminate scaling from the matrix
    mat._14 = 0.f;
    mat._24 = 0.f;
    mat._34 = 0.f;
    mat._44 = 1.f;
    return mat;
}
float4x4 Mat4_Identity()
{
    return float4x4(1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f);
}

VSOutput ShaderModelVS(VSInput input)
{
    VSModelInstanceData instance = InstanceBuffer[input.InstanceIndex];

    float3 normal = mul(Mat4_RotationPart(instance.transform), float4(input.normal, 1.f)).xyz;
    float3 world_sun_pos = normalize(float3(-0.5f, 0.5f, 1.f));
    float in_sun_coef = dot(world_sun_pos, normal);

    float4 color = float4(input.color, 1.0f);
    color *= instance.color;
    color.xyz *= clamp(in_sun_coef, 0.25f, 1.0f);

    float4x4 model_transform = instance.transform;
    model_transform = mul(CameraTransform, model_transform);

    VSOutput output;
    output.color = color;
    output.position = mul(model_transform, float4(input.position, 1.0f));
    return output;
}

float4 ShaderModelPS(VSOutput input) : SV_Target0
{
    return input.color;
}
