struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // ※本当はTEXCOORD1推奨
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // World位置は World で1回だけ
    float4 worldPos = mul(input.position, gTransformationMatrix.World);
    output.worldPosition = worldPos.xyz;

    // クリップ座標は input.position に WVP を1回だけ（Worldを二重にしない）
    output.position = mul(input.position, gTransformationMatrix.WVP);

    output.texcoord = input.texcoord;

    // 法線：逆転置で変換（w=0相当）
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));

    return output;
}
