
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
    float3 toEye = normalize(
    gCamera.worldPosition - input.worldPosition
);

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    // ★ 追加（資料の式そのまま）
    float3 reflectLight = reflect(gDirectionalLight.direction, normalize(input.normal));
    float RdotE = dot(reflectLight, toEye);
    float specularPow = pow(saturate(RdotE), gMaterial.shininess);

    if (gMaterial.enableLighting != 0)
    {
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float3 normal = normalize(input.normal);
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

        float3 diffuse =
    gMaterial.color.rgb *
    textureColor.rgb *
    gDirectionalLight.color.rgb *
    cos *
    gDirectionalLight.intensity;

        float3 specular =
    gDirectionalLight.color.rgb *
    gDirectionalLight.intensity *
    specularPow *
    float3(1.0f, 1.0f, 1.0f);
        
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
        
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    

    return output;
}

