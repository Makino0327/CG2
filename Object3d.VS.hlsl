cbuffer TimeBuffer : register(b1)
{
    float gTime;
    float3 padding;
}

cbuffer ViewProjection : register(b2)
{
    float4x4 viewProj;
}

StructuredBuffer<float> gStartTimes : register(t1);
StructuredBuffer<float2> gOffsets : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
    uint instanceID : SV_InstanceID;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float startTime = gStartTimes[input.instanceID];
    float timeSinceStart = gTime - startTime;

    // 出現前は描画しない
    if (timeSinceStart < 0.0f)
    {
        output.position = float4(0.0f, 0.0f, 0.0f, 0.0f); // clipされる
        output.texcoord = float2(0.0f, 0.0f);
        return output;
    }

    // 吸い込み進行度（0.0〜1.0）
    float lifeSpan = 5.0f;
    float t = saturate(timeSinceStart / lifeSpan);

    // スケール（中心に近づくほど小さく）
    float scale = lerp(1.0f, 0.0f, t);

    // 小さくなりすぎたら非表示
    if (scale < 0.01f)
    {
        output.position = float4(0.0f, 0.0f, 0.0f, 0.0f);
        output.texcoord = float2(0.0f, 0.0f);
        return output;
    }

    float2 startOffset = gOffsets[input.instanceID];

    // 中心への直線移動
    float2 currentOffset = lerp(startOffset, float2(0.0f, 0.0f), t);

    float3 offset;
    offset.x = currentOffset.x;
    offset.y = 0.0f;
    offset.z = currentOffset.y;

    // スケール適用
    float3 scaledPos = input.position.xyz * scale;
    float4 worldPosition = float4(scaledPos + offset, 1.0f);

    output.position = mul(worldPosition, viewProj);
    output.texcoord = input.texcoord;
    return output;
}
