
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
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
    int enableLighting;
    float shininess;
};

struct Camera
{
    float3 worldPosition;
};

ConstantBuffer<Camera> gCamera : register(b3);


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

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    // 法線
    float3 N = normalize(input.normal);

    // 光が「当たってくる方向」（あなたの拡散の式に合わせる）
    float3 L = normalize(-gDirectionalLight.direction);

    // 視線方向（ピクセル→カメラ）
    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    // HalfVector（スライドの式そのまま）
    float3 halfVector = normalize(L + toEye);

    // 拡散（Half-Lambertを使うなら今のままでOK）
    float NdotL = dot(N, L);
    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);

    float3 diffuse =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        cos *
        gDirectionalLight.intensity;

    // 鏡面（Blinn-Phong）
    float NdotH = dot(N, halfVector);
    float specularPow = pow(saturate(NdotH), gMaterial.shininess);

    float3 specular =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPow; // 必要なら * float3(1,1,1)

    output.color.rgb = diffuse + specular;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}



