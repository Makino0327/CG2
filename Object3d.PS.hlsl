// Object3D.PS.hlsl

#include "Object3d.hlsli" // VSOut がここに居るならインクルード

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

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor; // rgba (0..1)
    int gEnableLighting; // 0=None,1=Lambert,2=HalfLambert
    float3 _pad0; // 16B アライン
    float4x4 uvTransform;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ルートと合わせて b1
cbuffer DirectionalLightCB : register(b1)
{
    DirectionalLight gDirectionalLight;
};

struct PSOut
{
    float4 color : SV_TARGET0;
};

PSOut main(VSOut input) // ← 引数を受け取る
{
    PSOut o;

    // UV 変換後にサンプル
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);

    float3 N = normalize(input.normal);
    float3 L = normalize(-gDirectionalLight.direction);

    float3 baseRGB;

    if (gEnableLighting == 1)           // Lambert
    {
        float ndl = saturate(dot(N, L));
        baseRGB = gMaterialColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * ndl;
    }
    else if (gEnableLighting == 2)      // Half-Lambert
    {
        float ndl = dot(N, L);
        float hl = ndl * 0.5 + 0.5;
        baseRGB = gMaterialColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * hl * hl;
    }
    else // Unlit
    {
        baseRGB = gMaterialColor.rgb;
    }

    // 色はテクスチャを掛け、αは material.a * tex.a （←ブレンド用に正しい）
    float outA = saturate(gMaterialColor.a * tex.a);
    float3 outRGB = baseRGB * tex.rgb;

    o.color = float4(outRGB, outA);
    return o;
}
