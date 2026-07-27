#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float CalcLuminance(float3 color)
{
    // RGBを人間の目で見た明るさに近い重みで輝度へ変換する
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    static const float kPrewittHorizontalKernel[3][3] =
    {
        { -1.0f, 0.0f, 1.0f },
        { -1.0f, 0.0f, 1.0f },
        { -1.0f, 0.0f, 1.0f }
    };

    static const float kPrewittVerticalKernel[3][3] =
    {
        { -1.0f, -1.0f, -1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f }
    };

    float2 uvStepSize = float2(1.0f / 1280.0f, 1.0f / 720.0f);
    float2 difference = float2(0.0f, 0.0f);

    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            // 周囲の色の輝度差をPrewittフィルタで調べ、明暗の境目を輪郭にする
            float2 texcoord = input.texcoord + float2(x - 1, y - 1) * uvStepSize;
            float luminance = CalcLuminance(gTexture.Sample(gSampler, texcoord).rgb);
            difference.x += luminance * kPrewittHorizontalKernel[x][y];
            difference.y += luminance * kPrewittVerticalKernel[x][y];
        }
    }

    float4 baseColor = gTexture.Sample(gSampler, input.texcoord);
    float edge = saturate(length(difference) * 1.8f);

    // 明るい輪郭を足して、発射や爆発の瞬間を強調する
    output.color.rgb = saturate(baseColor.rgb + edge * float3(1.0f, 0.88f, 0.35f));
    output.color.a = baseColor.a;

    return output;
}