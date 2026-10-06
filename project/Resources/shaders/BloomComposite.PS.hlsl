#include "Bloom.hlsli"

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float4 base = gTexture.SampleLevel(gSampler, input.texcoord, 0);
    float3 light = gBloom.SampleLevel(gSampler, input.texcoord, 0).rgb;
    // ぼかす前の光から明るい部分を選び、輪郭を保った光の芯を作る。
    float3 emission = gEmission.SampleLevel(gSampler, input.texcoord, 0).rgb;
    float brightness = max(emission.r, max(emission.g, emission.b));
    float coreMask = smoothstep(0.35f, 1.20f, brightness);

    // 強い光の中心を白へ寄せ、橙色の周囲との明るさの差を付ける。
    float3 whiteCore = float3(brightness, brightness, brightness);
    float3 core = lerp(emission, whiteCore, coreMask * 0.75f);

    // 細い芯と周囲のにじみを別々に足し、大きさよりも中心の輝きを強める。
    return float4(base.rgb + light * intensity + core * coreMask * 0.65f, base.a);
}
