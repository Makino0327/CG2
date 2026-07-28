#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float3 ExtractBrightColor(float3 color)
{
    float brightness = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float mask = smoothstep(0.62f, 1.0f, brightness);
    return color * mask;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    float2 texel = float2(1.0f / 1280.0f, 1.0f / 720.0f);
    float4 baseColor = gTexture.Sample(gSampler, uv);

    float3 bloomColor = ExtractBrightColor(baseColor.rgb) * 0.24f;

    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2( 2.0f,  0.0f)).rgb) * 0.13f;
    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2(-2.0f,  0.0f)).rgb) * 0.13f;
    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2( 0.0f,  2.0f)).rgb) * 0.13f;
    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2( 0.0f, -2.0f)).rgb) * 0.13f;
    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2( 3.0f,  3.0f)).rgb) * 0.06f;
    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2(-3.0f,  3.0f)).rgb) * 0.06f;
    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2( 3.0f, -3.0f)).rgb) * 0.06f;
    bloomColor += ExtractBrightColor(gTexture.Sample(gSampler, uv + texel * float2(-3.0f, -3.0f)).rgb) * 0.06f;

    output.color = float4(saturate(baseColor.rgb + bloomColor * 0.65f), baseColor.a);
    return output;
}