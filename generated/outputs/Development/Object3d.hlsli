struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct DirectionalLight
{
    float4 color; // ライトの色
    float3 direction; // ライトの向き（単位ベクトル）
    float intensity; // 強度
};