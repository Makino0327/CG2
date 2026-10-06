struct Material {
    float4 color;
    int lightingType;
    float3 padding;
    float environmentCoefficient;
    float3 padding2;
    float4x4 uvTransform;
};
ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t1);
SamplerState gSampler : register(s0);
cbuffer DissolveParameter : register(b4) {
    float gDissolveThreshold;
    float gDissolveEdgeWidth;
    uint gDissolveEnabled;
    float gDissolvePadding;
    float4 gDissolveEdgeColor;
};
struct Input {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
void main(Input input) {
    // 透明なテクスチャ部分が、四角い影を落とさないようにする。
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    if (gTexture.Sample(gSampler, uv.xy).a * gMaterial.color.a <= 0.1f) { discard; }
    if (gDissolveEnabled != 0) {
        // 通常描画で消えた部分は、影からも同じ順序で消す。
        float mask = frac(sin(dot(input.texcoord, float2(12.9898f, 78.233f))) * 43758.5453f);
        if (mask < gDissolveThreshold) { discard; }
    }
    // 色は出力せず、頂点シェーダーの位置から計算された深度だけを書き込む。
}
