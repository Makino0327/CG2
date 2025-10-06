// Object3D.PS.hlsl  — 資料の方針に合わせて α を別計算

struct VSOut
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
    float4 color; // RGBA
    int lightingType; // 0:None 1:Lambert 2:HalfLambert
    float3 pad;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Lambert(float3 N, float3 L)
{
    return saturate(dot(N, L));
}
float HalfLambert(float3 N, float3 L)
{
    // (n·l)*0.5+0.5 の形を係数に、資料のようにガンマ寄りのpowを掛ける
    float ndotl = dot(N, L);
    return saturate(ndotl * 0.5 + 0.5);
}

float4 main(VSOut i) : SV_TARGET
{
    // UV変換 → サンプル
    float2 uv = mul(float4(i.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);

    // 法線とライト
    float3 N = normalize(i.normal);
    float3 L = normalize(-gDirectionalLight.direction);

    // ディフューズ係数
    float diff = 1.0f;
    if (gMaterial.lightingType == 1)
    {
        diff = pow(Lambert(N, L), 1.5f);
    }
    else if (gMaterial.lightingType == 2)
    {
        diff = pow(HalfLambert(N, L), 2.5f);
    }

    // ★ 資料通り：RGBにのみライティングを適用、αは別計算
    float3 rgb = gMaterial.color.rgb * tex.rgb;
    if (gMaterial.lightingType != 0)
    {
        rgb *= gDirectionalLight.color.rgb * diff * gDirectionalLight.intensity;
    }
    float a = gMaterial.color.a * tex.a;

    return float4(rgb, a);
}
