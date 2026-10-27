#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer VignetteData : register(b4)
{
    float gVignetteIntensity;
    float gLowHealth;
    float gTime;
    float gDamageNoise;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// フレームと画素の位置から、砂嵐と横方向の乱れを作る。
float DamageRandom(float2 position)
{
    return frac(sin(dot(position,float2(127.1f,311.7f)))*43758.5453f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    // 通常ビルドの古いシェーダーモデルでも使えるよう、UVから画素位置を求める。
    const float2 pixelPosition = input.texcoord*float2(1280.0f,720.0f);
    const float noiseFrame = floor(gTime*48.0f);
    if (gDamageNoise > 0.0f)
    {
        // 被弾直後だけ数ピクセル横に乱し、細い色ずれを加える。
        const float row = floor(pixelPosition.y/3.0f);
        uv.x += (DamageRandom(float2(row,noiseFrame))-0.5f)*gDamageNoise*0.008f;
    }
    uv = clamp(uv,0.001f,0.999f);
    output.color = gTexture.Sample(gSampler,uv);
    if (gDamageNoise > 0.0f)
    {
        const float shift = gDamageNoise*0.003f;
        output.color.r = gTexture.Sample(gSampler,clamp(uv+float2(shift,0),0.001f,0.999f)).r;
        output.color.b = gTexture.Sample(gSampler,clamp(uv-float2(shift,0),0.001f,0.999f)).b;
        const float grain = DamageRandom(floor(pixelPosition)+noiseFrame*float2(37.0f,19.0f))-0.5f;
        output.color.rgb = saturate(output.color.rgb+grain*gDamageNoise);
    }

    // 中心から離れるほど小さくなる値を作る
    float2 correct = input.texcoord * (1.0f - input.texcoord.yx);

    // 中心付近が1.0、端に近いほど0.0に近づく値に調整する
    float vignette = correct.x * correct.y * 16.0f;

    // powで暗くなり方を調整し、saturateで0.0から1.0の範囲に収める
    vignette = saturate(pow(vignette, 0.8f));

    // 元画像のRGBにヴィネッティング係数を掛けて、画面端を暗くする
    float vignetteRate = saturate(gVignetteIntensity);
    output.color.rgb *= lerp(1.0f, vignette, vignetteRate);

    // 二つの短い山で「ドクッ、ドクッ」という鼓動を作り、余韻を残して弱まらせる。
    const float phase = frac(gTime);
    const float2 beatDistance = (phase-float2(0.12f,0.31f))/float2(0.05f,0.075f);
    const float2 beats = exp(-beatDistance*beatDistance);
    const float heartbeat = saturate(beats.x+beats.y*0.65f);
    const float danger = saturate(gLowHealth);
    // 体力が減るほど赤い範囲と濃さを増やし、鼓動に合わせて縁が少し内側へ広がる。
    const float2 borderDistance = min(input.texcoord,1.0f-input.texcoord);
    const float borderWidth = lerp(0.12f,0.26f,danger)+heartbeat*0.018f*danger;
    const float border = 1.0f-smoothstep(0.0f,borderWidth,min(borderDistance.x,borderDistance.y));
    const float redOpacity = border*danger*(0.30f+0.42f*heartbeat);
    // 赤の明るさを抑え、暗い血の色に近い落ち着いた縁にする。
    output.color.rgb = lerp(output.color.rgb,float3(0.22f,0.002f,0.006f),redOpacity);

    return output;
}
