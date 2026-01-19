struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // ※安定させたいなら TEXCOORD1 推奨
};

struct DirectionalLight
{
    float4 color;
    float3 direction; // ★ライト→物体（光が進む向き）を入れる前提
    float intensity;
};

struct Material
{
    float4 color;
    int lightingType; // 0=None, 1=Lambert, 2=HalfLambert
    float shininess;
    float2 padding;
    float4x4 uvTransform;
};

struct Camera
{
    float3 worldPosition;
    float padding;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b3);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // ===== Texture =====
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, uv.xy);

    if (textureColor.a <= 0.01f)
    {
        discard;
    }

    // ===== ベクトル準備 =====
    float3 N = normalize(input.normal);

    // 視線（物体→カメラ）
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // ★重要：directionは「ライト→物体」前提なので反転して「物体→ライト」にする
    float3 L = normalize(-gDirectionalLight.direction);

    // ===== Diffuse =====
    float ndotl = saturate(dot(N, L));

    float diffuseFactor = 1.0f;
    if (gMaterial.lightingType == 1)
    {
        // Lambert
        diffuseFactor = ndotl;
    }
    else if (gMaterial.lightingType == 2)
    {
        // Half Lambert（安定版）
        diffuseFactor = ndotl * 0.5f + 0.5f;
    }

    float3 diffuse =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        diffuseFactor *
        gDirectionalLight.intensity;

    // ===== Specular =====
    // L は「物体→ライト」なので reflect(-L, N) が正しい
    float3 R = reflect(-L, N);

    float specAngle = saturate(dot(R, V));
    float specularPow = pow(specAngle, gMaterial.shininess);

    float3 specular =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPow;

    // ===== 合成 =====
    float3 finalColor =
        (gMaterial.lightingType == 0)
        ? (gMaterial.color.rgb * textureColor.rgb)
        : (diffuse + specular);

    output.color = float4(finalColor, gMaterial.color.a * textureColor.a);
    return output;
}
