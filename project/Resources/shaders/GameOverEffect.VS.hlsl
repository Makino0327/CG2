cbuffer Effect : register(b0)
{
    float time;
    float width;
    float height;
    float opacity;
};

struct Input
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float kind : TEXCOORD1;
};
struct Output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation float kind : TEXCOORD1;
};

Output main(Input input)
{
    Output output;
    // 画面内の帯と粒子を、そのまま正規化デバイス座標へ移す。
    output.position = float4(input.position.x / width * 2.0f - 1.0f,
        1.0f - input.position.y / height * 2.0f, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    output.kind = input.kind;
    return output;
}
