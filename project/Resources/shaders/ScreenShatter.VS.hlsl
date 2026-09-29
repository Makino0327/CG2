// CPUで計算した透視投影座標を使い、元のタイトル画像のUVを維持する。
struct Vertex {
    float4 position : POSITION0;
    float2 uv : TEXCOORD0;
    float3 barycentric : TEXCOORD1;
    float2 appearance : TEXCOORD2;
};
struct Output {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 barycentric : TEXCOORD1;
    float2 appearance : TEXCOORD2;
};
Output main(Vertex input) {
    Output output;
    output.position = input.position;
    output.uv = input.uv;
    output.barycentric = input.barycentric;
    output.appearance = input.appearance;
    return output;
}
