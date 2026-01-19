struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // まずワールド座標を作る
    float4 worldPos = mul(input.position, gTransformationMatrix.World); // ★追加
    output.worldPosition = worldPos.xyz; // ★追加

    // 画面座標
    output.position = mul(input.position, gTransformationMatrix.WVP);

    output.texcoord = input.texcoord;

    // 法線（とりあえずWorldで回す：非一様スケールが無いならこれでOK）
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.World));

    return output;
}
