#include "Bloom.hlsli"

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 stepUV = direction / float2(width, height);
    // 画素を飛び飛びに読まず、連続した画素でぼかして細い軌跡のまだら模様を防ぐ。
    float sigma = max(radius, 0.5f);
    int sampleRadius = min((int)ceil(sigma * 3.0f), 12);
    float3 light = 0.0f;
    float weightSum = 0.0f;
    [unroll]
    for (int index = -12; index <= 12; ++index) {
        if (abs(index) <= sampleRadius) {
            float weight = exp(-float(index * index) / (2.0f * sigma * sigma));
            light += gTexture.SampleLevel(gSampler, input.texcoord + stepUV * index, 0).rgb * weight;
            weightSum += weight;
        }
    }
    // ぼかしの広さを変えても、重みの合計で明るさを保つ。
    return float4(light / max(weightSum, 0.0001f), 1.0f);
}
