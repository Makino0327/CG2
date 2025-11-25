
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct Material
{
    float4 color;
    int lightingType;
    float3 padding;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);


struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 uv = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, uv.xy);

    if (textureColor.a <= 0.1f)
    {
        discard;
    }

    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction);

    float ndotl = saturate(dot(normal, lightDir));

    float lighting = 1.0f; // None用

    if (gMaterial.lightingType == 1)
    {
        // Lambert
        lighting = ndotl;
    }
    else if (gMaterial.lightingType == 2)
    {
        // Half Lambert
        lighting = pow(ndotl * 0.5f + 0.5f, 2.0f);
    }
    else
    {
        // None: ライト計算しない
        lighting = 1.0f;
    }

    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;

    float3 finalColor =
        (gMaterial.lightingType == 0)
        ? baseColor
        : baseColor * gDirectionalLight.color.rgb * gDirectionalLight.intensity * lighting;

    output.color = float4(finalColor, gMaterial.color.a * textureColor.a);
    return output;
}




