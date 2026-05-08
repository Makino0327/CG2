struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

cbuffer PerView : register(b0)
{
    float4x4 viewProjection;
    float4x4 billboardMatrix;
};

StructuredBuffer<Particle> gParticles : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    Particle particle = gParticles[instanceId];

    // billboard をベースに worldMatrix を作る
    float4x4 worldMatrix = billboardMatrix;

    worldMatrix[0][0] *= particle.scale.x;
    worldMatrix[0][1] *= particle.scale.x;
    worldMatrix[0][2] *= particle.scale.x;

    worldMatrix[1][0] *= particle.scale.y;
    worldMatrix[1][1] *= particle.scale.y;
    worldMatrix[1][2] *= particle.scale.y;

    worldMatrix[2][0] *= particle.scale.z;
    worldMatrix[2][1] *= particle.scale.z;
    worldMatrix[2][2] *= particle.scale.z;

    worldMatrix[3][0] = particle.translate.x;
    worldMatrix[3][1] = particle.translate.y;
    worldMatrix[3][2] = particle.translate.z;
    worldMatrix[3][3] = 1.0f;

    output.position = mul(input.position, mul(worldMatrix, viewProjection));
    output.texcoord = input.texcoord;
    output.color = particle.color;

    return output;
}
