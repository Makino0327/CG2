#include "Bloom.hlsli"

float3 ExtractLight(float3 color)
{
    // オレンジ色の弾も抽出できるよう、RGBの最も明るい成分で判定する。
    float brightness = max(color.r, max(color.g, color.b));
    float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / max(4.0f * knee, 0.0001f);
    float contribution = max(brightness - threshold, soft) / max(brightness, 0.0001f);
    return color * contribution;
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    uint2 basePixel = uint2(input.position.xy) * 2;
    float3 light = 0.0f;
    // 先に各画素から光を抽出してから縮小し、細い弾が平均化で消えるのを防ぐ。
    [unroll]
    for (uint y = 0; y < 2; ++y) {
        [unroll]
        for (uint x = 0; x < 2; ++x) {
            uint2 pixel = min(basePixel + uint2(x, y), uint2(width - 1, height - 1));
            light += ExtractLight(gTexture.Load(int3(pixel, 0)).rgb);
        }
    }
    return float4(light * 0.25f, 1.0f);
}
