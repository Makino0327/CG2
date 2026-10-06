#ifndef SHADOW_MAP_HLSLI
#define SHADOW_MAP_HLSLI

// C++側のShadowMap::Parametersと同じ配置にする。
cbuffer ShadowParameters : register(b5) {
    float4x4 lightViewProjection;
    float3 shadowLightDirection;
    float shadowStrength;
    float2 shadowTexelSize;
    float shadowDepthBias;
    float shadowEnabled;
};
Texture2D<float> gShadowMap : register(t3);
SamplerComparisonState gShadowSampler : register(s1);

float GetShadowVisibility(float3 worldPosition, float3 normal) {
    if (shadowEnabled < 0.5f) { return 1.0f; }
    float4 projected = mul(float4(worldPosition, 1.0f), lightViewProjection);
    float3 position = projected.xyz / projected.w;
    float2 uv = float2(position.x * 0.5f + 0.5f, -position.y * 0.5f + 0.5f);

    // 影用カメラの範囲外を、黒い影で埋めない。
    if (any(uv < 0.0f) || any(uv > 1.0f) || position.z < 0.0f || position.z > 1.0f) {
        return 1.0f;
    }
    // 斜めの面では少し余裕を増やし、自分自身の深度との誤差による縞模様を抑える。
    float slope = 1.0f - saturate(dot(normal, -shadowLightDirection));
    float comparisonDepth = position.z - shadowDepthBias * (1.0f + slope * 2.0f);
    float visibility = 0.0f;
    // 周囲9点の深度比較を平均し、影の輪郭を少し柔らかくする。
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            visibility += gShadowMap.SampleCmpLevelZero(
                gShadowSampler, uv + float2(x, y) * shadowTexelSize, comparisonDepth);
        }
    }
    // 影の中にもわずかな明るさを残し、モデルの形を読めるようにする。
    return lerp(1.0f - shadowStrength, 1.0f, visibility / 9.0f);
}
#endif
