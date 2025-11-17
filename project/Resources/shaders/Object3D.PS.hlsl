
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

    // UV & テクスチャ
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // マテリアル × テクスチャ
    float4 baseColor = gMaterial.color * textureColor;

    // 法線とライト方向
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction);

    float NdotL = dot(normal, lightDir);

    // ライティング係数
    float lightTerm = 1.0f; // 0: Unlit 相当

    if (gMaterial.lightingType == 1)
    {
        // Lambert
        lightTerm = saturate(NdotL); // 0～1
    }
    else if (gMaterial.lightingType == 2)
    {
        // Half Lambert
        lightTerm = NdotL * 0.5f + 0.5f; // 0～1
        // もう少し暗くしたければ:
        // lightTerm = pow(lightTerm, 1.5f);
    }

    float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity;

    float3 finalColor = baseColor.rgb * lightColor * lightTerm;

    // 範囲を0～1に抑えたいなら saturate してもOK
    finalColor = saturate(finalColor);

    output.color.rgb = finalColor;
    output.color.a = baseColor.a;
    return output;
}



