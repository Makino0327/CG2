#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gMaskTexture : register(t2);
SamplerState gSampler : register(s0);

// ディゾルブ用の定数
cbuffer DissolveParameter : register(b1)
{
    float threshold; // 進行度
    float edgeWidth; // 境界の幅
    float2 padding; // 16byte揃え
    float4 edgeColor; // 境界色
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 baseColor = gTexture.Sample(gSampler, input.texcoord);
    float mask = gMaskTexture.Sample(gSampler, input.texcoord).r;

    // 閾値より小さい部分は消す
    if (mask < threshold)
    {
        discard;
    }

    // 境界付近だけ色を足す
    float edge = 1.0f - smoothstep(threshold, threshold + edgeWidth, mask);

    output.color = baseColor;
    output.color.rgb += edge * edgeColor.rgb;
    output.color.a = baseColor.a;

    return output;
}
