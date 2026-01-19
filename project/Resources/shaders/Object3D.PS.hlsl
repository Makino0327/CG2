struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // 安定させたいなら TEXCOORD1 推奨
};

struct DirectionalLight
{
    float4 color;
    float3 direction; // ここは「ライトが向いている方向」想定
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

    // ===== Texture + UVTransform =====
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, uv.xy);

    // 透明抜き
    if (textureColor.a <= 0.01f)
    {
        discard;
    }

    // Lightingなし
    if (gMaterial.lightingType == 0)
    {
        output.color = float4(gMaterial.color.rgb * textureColor.rgb,
                              gMaterial.color.a * textureColor.a);
        return output;
    }

    // ===== ベクトル準備 =====
    float3 N = normalize(input.normal);

    // 元コードに合わせて「当たってくる方向」を作る
    // 影が反対なら、ここを L = normalize(gDirectionalLight.direction); に変える
    float3 L = normalize(-gDirectionalLight.direction);

    // 視線（ピクセル→カメラ）
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // HalfVector（Blinn-Phong）
    float3 H = normalize(L + V);

    // ===== Diffuse =====
    float ndotl = dot(N, L);

    float diffuseFactor = 1.0f;
    if (gMaterial.lightingType == 1)
    {
        // Lambert
        diffuseFactor = saturate(ndotl);
    }
    else if (gMaterial.lightingType == 2)
    {
        // Half-Lambert（元コードの形に寄せる）
        // ※見た目を合わせたいなら pow を入れる（例: ^2）
        diffuseFactor = saturate(ndotl * 0.5f + 0.5f);
        // diffuseFactor = pow(diffuseFactor, 2.0f); // ←必要ならON
    }

    float3 diffuse =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        diffuseFactor *
        gDirectionalLight.intensity;

    // ===== Specular（Blinn-Phong）=====
    float ndoth = saturate(dot(N, H));
    float specularPow = pow(ndoth, gMaterial.shininess);

    float3 specular =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPow;

    // ===== 合成 =====
    float3 finalColor = diffuse + specular;

    output.color = float4(finalColor, gMaterial.color.a * textureColor.a);
    return output;
}
