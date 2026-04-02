struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

// ▼ ここを追加（Object 用 PS と同じでOK）
struct Material
{
    float4 color;
    int enableLighting; // 使わなくてもそのまま
    float3 padding;
};


Texture2D<float4> gTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV 変換しない場合（スライドでは uvTransform の話はしない）
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // Color × Texture
    output.color = textureColor;
    
    output.color = textureColor * input.color;


    // αが0なら描画しない
    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}
