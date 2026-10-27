#include "CopyImage.hlsli"

Texture2D<float4> scene : register(t0);
SamplerState sceneSampler : register(s0);
cbuffer DeathBlur : register(b0)
{
    float radius;
    float darkness;
    float vignette;
    float padding;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width, height;
    scene.GetDimensions(width,height);
    const float2 texel = 1.0f/float2(width,height);
    // 色を保ったまま、視界のピントが徐々に外れるようにぼかす。
    float3 blurred = 0.0f;
    float total = 0.0f;
    [unroll] for (int y = -2; y <= 2; ++y)
    {
        [unroll] for (int x = -2; x <= 2; ++x)
        {
            const float weight = exp(-float(x*x+y*y)/3.2f);
            const float2 uv = clamp(input.texcoord+float2(x,y)*texel*radius*0.5f,texel,1.0f-texel);
            blurred += scene.Sample(sceneSampler,uv).rgb*weight;
            total += weight;
        }
    }
    const float2 center = (input.texcoord-0.5f)*2.0f;
    const float edge = smoothstep(0.25f,1.45f,length(center));
    const float brightness = (1.0f-darkness)*(1.0f-edge*vignette);
    return float4(blurred/total*brightness,1.0f);
}
