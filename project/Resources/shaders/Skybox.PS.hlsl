#include "Skybox.hlsli"

cbuffer Material : register(b1)
{
    float4 color;
};

TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    // 背景の空も暗くして、弾の光との明暗差を付ける
    output.color = textureColor * color;
    output.color.rgb *= 0.30f;
    return output;
}
