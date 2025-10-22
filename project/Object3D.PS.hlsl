
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
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
    int lightingType;
    float3 padding;
    float4x4 uvTransform;
};

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
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    float3 finalColor = gMaterial.color.rgb * textureColor.rgb;

    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction);
    
    float NdoL = dot(normalize(input.normal), -gDirectionalLight.direction);
    float cos = pow(NdoL * 0.5 + 0.5, 2.0);

    if (gMaterial.lightingType == 1)
    {
        // Lambert（やや暗め）
        float ndotl = saturate(dot(normal, lightDir));
        float lambert = pow(ndotl, 1.5f); // 少しシャープに
        finalColor *= gDirectionalLight.color.rgb * gDirectionalLight.intensity * lambert * 0.5f;
    }
    else if (gMaterial.lightingType == 2)
    {
        // Half Lambert（かなり暗めに調整）
        float ndotl = dot(normal, lightDir);
        float halfLambert = pow(ndotl * 0.5f + 0.5f, 2.5f);
        finalColor *= gDirectionalLight.color.rgb * gDirectionalLight.intensity * halfLambert * 0.4f;
    }
    output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    output.color.a = gMaterial.color.a * textureColor.a;

    //output.color = gMaterial.color *textureColor* gDirectionalLight.color * cos * gDirectionalLight.intensity;
    //output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    return output;
}



