#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ランダムノイズ用の定数
cbuffer RandomNoiseParameter : register(b2)
{
    float intensity; // ノイズの濃さ
    float time; // 時間で乱数を変化させる
    float speed; // C++側とレイアウトを合わせる
    float padding; // 16byte揃え
};


struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 2次元座標から 0.0f 以上 1.0f 未満の乱数風の値を作る
float rand2dTo1d(float2 value)
{
    float2 smallValue = sin(value);
    float random = frac(sin(dot(smallValue, float2(12.9898f, 78.233f))) * 43758.5453f);
    return random;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 baseColor = gTexture.Sample(gSampler, input.texcoord);

    // 時間を混ぜて毎フレーム少しずつ違うノイズにする
    float random = rand2dTo1d(input.texcoord * 500.0f + time);

    // 0.5 を中心にして明暗両方向へ効かせる
    float noise = (random - 0.5f) * 2.0f * intensity;

    output.color.rgb = saturate(baseColor.rgb + noise);
    output.color.a = baseColor.a;
    return output;
}
