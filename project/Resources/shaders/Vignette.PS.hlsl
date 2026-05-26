#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 元画像の色を取得する
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 画面中央から離れるほど値が小さくなる係数を作る
    float2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    float vignette = correct.x * correct.y * 16.0f;
    vignette = saturate(pow(vignette, 0.8f));

    // 端に近いほど強くなる赤ビネット用の強さ
    float edge = 1.0f - vignette;

    // 被弾感が出るように少し濃い赤を使う
    float3 damageColor = float3(0.9f, 0.0f, 0.0f);

    // 元画像を少し暗くしつつ赤を混ぜる
    output.color.rgb *= lerp(0.55f, 1.0f, vignette);
    output.color.rgb = lerp(output.color.rgb, damageColor, edge * 0.6f);

    return output;
}
