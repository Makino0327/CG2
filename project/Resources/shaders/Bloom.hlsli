#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gBloom : register(t1);
// ぼかす前の発光画像を使い、細く明るい光の芯を残す。
Texture2D<float4> gEmission : register(t2);
SamplerState gSampler : register(s0);

cbuffer BloomParameters : register(b0)
{
    float threshold;  // 光として抽出するしきい値。
    float knee;       // しきい値付近の切り替わりを滑らかにする幅。
    float intensity;  // 光を足す強さ。
    float radius;     // ぼかしの広がり。
    float2 direction; // 横または縦のぼかし方向。
    float2 padding;
};
