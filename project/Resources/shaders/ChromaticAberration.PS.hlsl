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

    float2 uv = input.texcoord;
    float2 fromCenter = uv - float2(0.5f, 0.5f);
    float distanceFromCenter = saturate(length(fromCenter) * 1.4f);
    float2 direction = distanceFromCenter > 0.0001f
        ? normalize(fromCenter)
        : float2(0.0f, 0.0f);

    float2 offset = direction * (0.0025f + distanceFromCenter * 0.0040f);
    float red = gTexture.Sample(gSampler, uv + offset).r;
    float green = gTexture.Sample(gSampler, uv).g;
    float blue = gTexture.Sample(gSampler, uv - offset).b;
    float alpha = gTexture.Sample(gSampler, uv).a;

    output.color = float4(red, green, blue, alpha);
    return output;
}