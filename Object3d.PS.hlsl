// Object3D.PS.hlsl  — 資料の方針に合わせて α を別計算

struct VSOut
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

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float3 padding;
    float4x4 uvTransform;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ★ b1 に合わせる（ルートと一致）
cbuffer DirectionalLightCB : register(b1)
{
    DirectionalLight gDirectionalLight;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main()
{
    PixelShaderOutput output;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);

    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction);

    float3 litColor;
    if (gEnableLighting == 1)
    { // Lambert
        float NdotL = saturate(dot(normal, lightDir));
        litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
        output.color = float4(litColor, gMaterialColor.a) * tex; // ★ Aも掛ける
    }
    else if (gEnableLighting == 2)
    { // Half-Lambert
        float NdotL = dot(normal, lightDir);
        float halfLambert = NdotL * 0.5 + 0.5;
        litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * halfLambert * halfLambert;
        output.color = float4(litColor, gMaterialColor.a) * tex; // ★
    }
    else
    {
        output.color = gMaterialColor * tex; // ここはそのままでOK
    }
    return output;
}
