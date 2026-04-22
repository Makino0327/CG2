#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 円周率をHLSL内で使うために定義する
static const float PI = 3.14159265f;

// ガウス関数で、中心からの距離に応じた重みを計算する
float Gauss(float x, float y, float sigma)
{
    // eの指数部分を計算する
    float exponent = -((x * x) + (y * y)) / (2.0f * sigma * sigma);

    // 分母の 2πσ^2 を計算する
    float denominator = 2.0f * PI * sigma * sigma;

    // expはeの累乗を計算するHLSLの関数
    return exp(exponent) / denominator;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 中心から見た周囲9マスの相対位置
    static const float2 kIndex3x3[3][3] =
    {
        { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
        { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } },
    };

    // 1テクセル分のUV移動量を画面サイズから計算する
    float2 uvStepSize = float2(1.0f / 1280.0f, 1.0f / 720.0f);

    // ぼかしの広がりを決める値
    float sigma = 2.0f;

    // 合計色と重み合計を初期化する
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float weight = 0.0f;

    // 周囲3x3のテクセルを取得して、ガウス関数の重みで加算する
    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            // 現在参照するマスの中心からの相対位置を取得する
            float2 index = kIndex3x3[x][y];

            // 相対位置からガウス関数の重みを計算する
            float kernel = Gauss(index.x, index.y, sigma);

            // 現在のUVから、周囲テクセル分だけずらしたUVを作る
            float2 texcoord = input.texcoord + index * uvStepSize;

            // ずらしたUV位置の色を取得する
            float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;

            // 取得した色にガウス重みを掛けて加算する
            color += fetchColor * kernel;

            // 最後に正規化するため、重みの合計も足しておく
            weight += kernel;
        }
    }

    // 重みの合計で割って、明るさが変わりすぎないようにする
    output.color.rgb = color / weight;

    // アルファ値は元画像の中心ピクセルをそのまま使う
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;

    return output;
}
