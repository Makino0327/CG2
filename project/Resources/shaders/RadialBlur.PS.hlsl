#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ラジアルブラー用の定数
cbuffer RadialBlurParameter : register(b0)
{
    float2 kCenter; // ブラーの中心UV
    float kBlurWidth; // ブラーの強さ
    float padding; // 16byte揃え
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // サンプリング回数
    const int kNumSamples = 10;

    // 中心から現在のUVへ向かう方向を求める
    float2 direction = input.texcoord - kCenter;

    // 加算用の色
    float3 outputColor = float3(0.0f, 0.0f, 0.0f);

    // 中心方向へ少しずつずらしながら色を集める
    for (int sampleIndex = 0; sampleIndex < kNumSamples; ++sampleIndex)
    {
        float t = float(sampleIndex) / float(kNumSamples - 1);
        float2 texcoord = input.texcoord - direction * kBlurWidth * t;
        outputColor += gTexture.Sample(gSampler, texcoord).rgb;
    }

    // 集めた色を平均する
    output.color.rgb = outputColor / float(kNumSamples);

    // アルファ値は元画像の値をそのまま使う
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;

    return output;
}
