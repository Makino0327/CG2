#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    static const float kPrewittHorizontalKernel[3][3] =
    {
        { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
        { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
        { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
    };

    static const float kPrewittVerticalKernel[3][3] =
    {
        { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
        { 0.0f, 0.0f, 0.0f },
        { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
    };

    float2 uvStepSize = float2(1.0f / 1280.0f, 1.0f / 720.0f);
    float2 difference = float2(0.0f, 0.0f);

    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            float2 texcoord = input.texcoord + float2(x - 1, y - 1) * uvStepSize;
            float depth = gDepthTexture.Sample(gSamplerPoint, texcoord);
            difference.x += depth * kPrewittHorizontalKernel[x][y];
            difference.y += depth * kPrewittVerticalKernel[x][y];
        }
    }

    float weight = saturate(length(difference) * 100.0f); // // 見えやすいように少し強める
    float3 color = gTexture.Sample(gSampler, input.texcoord).rgb;

    output.color.rgb = (1.0f - weight) * color; // // エッジが強いほど黒くする
    output.color.a = 1.0f;

    return output;
}
