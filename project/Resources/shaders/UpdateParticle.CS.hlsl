static const uint kMaxParticles = 10000;

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
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<int> gFreeList : register(u2);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;

    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // 生きているParticleだけ更新する
    if (gParticles[particleIndex].color.a != 0.0f)
    {
        gParticles[particleIndex].translate +=
            gParticles[particleIndex].velocity * gPerFrame.deltaTime;

        gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

        float alpha =
            1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);

        gParticles[particleIndex].color.a = saturate(alpha);

        // 死んだParticleは描画されないようにしつつFreeListへ戻す
        if (gParticles[particleIndex].color.a == 0.0f)
        {
            gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);

            int freeListIndex;
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

            // 先頭を1つ進めた位置に、このParticleIndexを返却する
            if ((freeListIndex + 1) < (int) kMaxParticles)
            {
                gFreeList[freeListIndex + 1] = particleIndex;
            }
            else
            {
                // 想定外に溢れたときは安全のため元に戻す
                InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
            }
        }
    }
}
