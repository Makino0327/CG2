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

struct PointLight
{
    float4 color; // ライトの色
    float3 position; // ライトの位置
    float intensity; // 強度
    float radius; // 届く最大距離
    float decay; // 減衰率（大きいほど急に減衰）
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // ------------------------------
    // ライティングなし
    // ------------------------------
    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    // 法線
    float3 N = normalize(input.normal);

    // 視線方向（surface -> camera）
    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    // ==================================================
    // DirectionalLight（元の処理そのまま）
    // ==================================================
    float3 L = normalize(-gDirectionalLight.direction);

    float3 halfVector = normalize(L + toEye);

    float NdotL = dot(N, L);
    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

    float3 diffuse =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        cos *
        gDirectionalLight.intensity;

    float NdotH = dot(N, halfVector);
    float specularPow = pow(saturate(NdotH), gMaterial.shininess);

    float3 specular =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPow;

    // ==================================================
    // PointLight（減衰計算：資料どおり）
    // ==================================================

    // 距離（資料どおり）
    float distance = length(gPointLight.position - input.worldPosition);

    // factor（資料どおり）
    float factor = pow(
        saturate(-distance / gPointLight.radius + 1.0f),
        gPointLight.decay
    );

    // 光が当たってくる方向（light -> surface）※スライドの形
    float3 Lp = normalize(input.worldPosition - gPointLight.position);

    float3 halfVectorP = normalize(Lp + toEye);

    // 拡散（Half-Lambert）
    float NdotLp = dot(N, Lp);
    float cosP = pow(NdotLp * 0.5f + 0.5f, 2.0f);

    float3 diffusePoint =
        gMaterial.color.rgb *
        textureColor.rgb *
        gPointLight.color.rgb *
        cosP *
        gPointLight.intensity *
        factor; // ★減衰

    // 鏡面（Blinn-Phong）
    float NdotHp = dot(N, halfVectorP);
    float specularPowP = pow(saturate(NdotHp), gMaterial.shininess);

    float3 specularPoint =
        gPointLight.color.rgb *
        gPointLight.intensity *
        specularPowP *
        factor; // ★減衰

    // 足し算（Directional + Point）
    diffuse += diffusePoint;
    specular += specularPoint;

    // ------------------------------
    // 出力
    // ------------------------------
    output.color.rgb = diffuse + specular;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}
