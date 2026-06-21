#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// HP低下と死亡暗転に使うビネット設定
cbuffer VignetteParameter : register(b0)
{
    float4 gVignetteColor;
    float gVignetteIntensity;
    float gDarkness;
    float2 gVignettePadding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 画面中央から端へ向かって強くなるマスクを作る
    float2 centeredUv = input.texcoord * 2.0f - 1.0f;
    float edgeMask = saturate(length(centeredUv) - 0.25f);
    edgeMask = smoothstep(0.0f, 0.85f, edgeMask);

    // 画面端ほど元画像へ赤色を混ぜる
    float redAmount = edgeMask * saturate(gVignetteIntensity);
    float3 darkenedScene = output.color.rgb * 0.45f;
    float3 redScene = darkenedScene + gVignetteColor.rgb * 0.55f;
    output.color.rgb = lerp(output.color.rgb, redScene, redAmount);

    // 死亡時は画面全体を徐々に暗くして目がくらむ感覚を作る
    output.color.rgb *= 1.0f - saturate(gDarkness);

    return output;
}
