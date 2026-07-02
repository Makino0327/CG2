#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer ShockwaveParameter : register(b3)
{
    float2 kCenter;
    float kRadius;
    float kThickness;
    float kStrength;
    float kProgress;
    float kAspectRatio;
    float kWhiteWave;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    float2 delta = uv - kCenter;

    float2 aspectDelta = float2(delta.x * kAspectRatio, delta.y);
    float distanceFromCenter = length(aspectDelta);

    float distanceToRing = abs(distanceFromCenter - kRadius);
    float ring = 1.0f - saturate(distanceToRing / max(kThickness, 0.0001f));
    ring *= ring;

    float2 direction = float2(0.0f, 0.0f);
    if (distanceFromCenter > 0.0001f)
    {
        float2 aspectDirection = aspectDelta / distanceFromCenter;
        direction = float2(aspectDirection.x / kAspectRatio, aspectDirection.y);
    }

    float fade = 1.0f - saturate(kProgress);
    float2 distortion = direction * ring * kStrength * fade;
    float2 distortedUV = saturate(uv - distortion);

    float4 color = gTexture.Sample(gSampler, distortedUV);
    color.rgb += ring * fade * 0.08f * kWhiteWave;

    output.color = color;
    return output;
}