struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};


VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    output.position =
        mul(input.position, gParticle[instanceId].WVP);

    output.texcoord = input.texcoord;
    
    output.color = gParticle[instanceId].color;

    return output;
}
