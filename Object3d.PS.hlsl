
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct Material
{
    float4 color;
    int enableLighting;
    float shininess;
};

struct Camera
{
    float3 worldPosition;
};

ConstantBuffer<Camera> gCamera : register(b3);


ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);


struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 tex = gTexture.Sample(gSampler, input.texcoord);

    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * tex;
        return output;
    }

    float3 N = normalize(input.normal);

    // あなたの既存の拡散計算に合わせる（Half-Lambert）
    float3 L = normalize(-gDirectionalLight.direction); // 光が「当たってくる方向」
    float NdotL = dot(N, L);
    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

    // 視線
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // 反射（入射 = -L）
    float3 R = reflect(-L, N);
    float specularPow = pow(saturate(dot(R, V)), gMaterial.shininess);

    float3 diffuse =
        gMaterial.color.rgb * tex.rgb *
        gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;

    float3 specular =
        gDirectionalLight.color.rgb * gDirectionalLight.intensity *
        specularPow;

    output.color.rgb = diffuse + specular;
    output.color.a = gMaterial.color.a * tex.a;
    return output;
}


