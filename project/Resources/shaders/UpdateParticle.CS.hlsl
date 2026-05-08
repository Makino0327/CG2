static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

cbuffer PerFrameBuffer : register(b0)
{
    PerFrame gPerFrame;
};

RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(256, 1, 1)]

void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;

    if (particleIndex < kMaxParticles)
    {
        // alphaが0のparticleは死んでいるとみなして更新しない
        if (gParticles[particleIndex].color.a != 0.0f)
        {
            gParticles[particleIndex].translate +=
                gParticles[particleIndex].velocity * gPerFrame.deltaTime;

            gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

            float alpha =
                1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);

            gParticles[particleIndex].color.a = saturate(alpha);

            // 完全に消えたらscaleも0にして未使用扱いに寄せる
            if (gParticles[particleIndex].color.a == 0.0f)
            {
                gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
            }
        }
    }
}
