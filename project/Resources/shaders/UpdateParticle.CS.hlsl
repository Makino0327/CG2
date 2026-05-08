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

    // 範囲外のthreadは何もしない
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // 今使っているParticleを取り出す
    Particle particle = gParticles[particleIndex];

    // scaleが0なら未使用扱いにして更新しない
    if (particle.scale.x == 0.0f &&
        particle.scale.y == 0.0f &&
        particle.scale.z == 0.0f)
    {
        return;
    }

    // 経過時間を進める
    particle.currentTime += gPerFrame.deltaTime;

    // 寿命を超えたら消す
    if (particle.currentTime >= particle.lifeTime)
    {
        particle.translate = float3(0.0f, 0.0f, 0.0f);
        particle.scale = float3(0.0f, 0.0f, 0.0f);
        particle.lifeTime = 0.0f;
        particle.velocity = float3(0.0f, 0.0f, 0.0f);
        particle.currentTime = 0.0f;
        particle.color = float4(0.0f, 0.0f, 0.0f, 0.0f);

        gParticles[particleIndex] = particle;
        return;
    }

    // 位置を進める
    particle.translate += particle.velocity * gPerFrame.deltaTime;

    // 寿命に応じて少しずつ透明にする
    {
        float t = particle.currentTime / particle.lifeTime;
        t = saturate(t);
        particle.color.a = 1.0f - t;
    }

    // 更新結果を書き戻す
    gParticles[particleIndex] = particle;
}
