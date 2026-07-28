#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer VignetteData : register(b4)
{
    float gVignetteIntensity;
    float3 padding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 元画像の色をテクスチャから取得する
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 中心から離れるほど小さくなる値を作る
    float2 correct = input.texcoord * (1.0f - input.texcoord.yx);

    // 中心付近が1.0、端に近いほど0.0に近づく値に調整する
    float vignette = correct.x * correct.y * 16.0f;

    // powに渡す値を0.0から1.0に収めて、負の値による警告を防ぐ
    vignette = saturate(pow(saturate(vignette), 0.8f));

    // 元画像のRGBにヴィネッティング係数を掛けて、画面端を暗くする
    float vignetteRate = saturate(gVignetteIntensity);
    output.color.rgb *= lerp(1.0f, vignette, vignetteRate);

    return output;
}
