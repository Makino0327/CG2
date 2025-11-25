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
    int lightingType; // 0: None(Unlit), 1: Lambert, 2: HalfLambert
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

    // ----------------------------
    // UV変換 & テクスチャ取得
    // ----------------------------
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // ============================
    // ★ 黒背景（カラーキー）を抜く
    // fence.png は黒背景で α が常に1なので
    // α判定では抜けない → 色で判定してdiscard
    // ============================
    if (textureColor.r < 0.1f &&
        textureColor.g < 0.1f &&
        textureColor.b < 0.1f)
    {
        discard; // 背景を完全に消す（穴になる）
    }

    // ----------------------------
    // マテリアル × テクスチャ
    // ----------------------------
    float4 baseColor = gMaterial.color * textureColor;

    // ----------------------------
    // ライティング
    // ----------------------------
    float3 finalColor;

    // 0: None（Unlit）→ ライトを掛けずにそのまま出す
    if (gMaterial.lightingType == 0)
    {
        finalColor = baseColor.rgb;
    }
    else
    {
        // 法線とライト方向
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float NdotL = dot(normal, lightDir);

        // ライティング係数
        float lightTerm = 1.0f;

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

        finalColor = baseColor.rgb * lightColor * lightTerm;
    }

    // 0～1に抑える
    finalColor = saturate(finalColor);

    output.color.rgb = finalColor;
    output.color.a = baseColor.a; // 半透明にしたいなら *0.5f とか

    return output;
}
