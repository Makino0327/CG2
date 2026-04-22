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

    // 5x5の各テクセルを同じ重みで平均するためのカーネル
    static const float kKernel5x5[5][5] =
    {
        { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
        { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
        { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
        { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
        { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    };

    // 1テクセル分のUV移動量を画面サイズから計算する
    float2 uvStepSize = float2(1.0f / 1280.0f, 1.0f / 720.0f);

    // 25マス分の色を足していくため、最初は0で初期化する
    output.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    // 周囲5x5のテクセルを取得して平均色を作る
    for (int x = 0; x < 5; ++x)
    {
        for (int y = 0; y < 5; ++y)
        {
            // x,yの0から4を、-2から2の範囲に変換する
            float2 index = float2(float(x - 2), float(y - 2));

            // 現在のUVから、周囲テクセル分だけずらしたUVを作る
            float2 texcoord = input.texcoord + index * uvStepSize;

            // ずらしたUV位置の色を取得する
            float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;

            // 取得した色にカーネルの重みを掛けて加算する
            output.color.rgb += fetchColor * kKernel5x5[x][y];
        }
    }

    // アルファ値は元画像の中心ピクセルをそのまま使う
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;

    return output;
}
