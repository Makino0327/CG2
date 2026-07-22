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

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<int> gFreeList : register(u2);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;

    // 範囲外のthreadは何もしない
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // Particleを初期化する
    gParticles[particleIndex] = (Particle) 0;
    gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
    gParticles[particleIndex].color = float4(1.0f, 1.0f, 1.0f, 0.0f);

    // 空いているIndexをそのままFreeListに順番に入れる
    gFreeList[particleIndex] = particleIndex;

    // FreeListの先頭は末尾を指すようにする
    if (particleIndex == 0)
    {
        gFreeListIndex[0] = kMaxParticles - 1;
    }
}
