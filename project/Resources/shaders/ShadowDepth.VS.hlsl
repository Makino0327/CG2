#include "ShadowMap.hlsli"

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b2);
struct Input {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};
struct Output {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
Output main(Input input) {
    Output output;
    // 通常描画と同じ変形済み頂点を使い、アニメーション中の姿も影へ反映する。
    float4 worldPosition = mul(input.position, gTransformationMatrix.World);
    output.position = mul(worldPosition, lightViewProjection);
    output.texcoord = input.texcoord;
    return output;
}
