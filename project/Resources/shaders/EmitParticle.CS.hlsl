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

struct EmitterSphere
{
    float3 translate;
    float radius;

    uint count;
    float frequency;
    float frequencyTime;
    uint emit;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

cbuffer EmitterSphereBuffer : register(b0)
{
    EmitterSphere gEmitter;
};

cbuffer PerFrameBuffer : register(b1)
{
    PerFrame gPerFrame;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeCounter : register(u1);

// 0.0f～1.0f の乱数を返す
float rand1dTo1d(float value)
{
    return frac(sin(value) * 43758.5453f);
}

// 1つの値から3成分の乱数を作る
float3 rand1dTo3d(float value)
{
    return float3(
        rand1dTo1d(value + 0.0f),
        rand1dTo1d(value + 1.0f),
        rand1dTo1d(value + 2.0f)
    );
}

// float3をseedにして0.0f～1.0fの乱数float3を返す
float3 rand3dTo3d(float3 value)
{
    float x = dot(value, float3(127.1f, 311.7f, 74.7f));
    float y = dot(value, float3(269.5f, 183.3f, 246.1f));
    float z = dot(value, float3(113.5f, 271.9f, 124.6f));

    return frac(sin(float3(x, y, z)) * 43758.5453f);
}

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 今フレーム発生しないなら何もしない
    if (gEmitter.emit == 0)
    {
        return;
    }

    // 時間とthread idからベースseedを作る
    float3 seed = rand1dTo3d(gPerFrame.time + DTid.x * 0.1234f);

    // count個ぶんParticleを発生させる
    for (uint countIndex = 0; countIndex < gEmitter.count; ++countIndex)
    {
        int particleIndex;

        // Counterを安全に進めて、進める前の値を受け取る
        InterlockedAdd(gFreeCounter[0], 1, particleIndex);

        // 最大数を超えたら何もしない
        if (particleIndex >= (int) kMaxParticles)
        {
            continue;
        }

        // countIndexごとに別のseedになるように少し混ぜる
        float3 localSeed = seed + float3(
            (float) countIndex * 0.11f,
            (float) countIndex * 0.17f,
            (float) countIndex * 0.23f
        );

        // 発生位置用乱数
        float3 randomPos = rand3dTo3d(localSeed);
        randomPos = randomPos * 2.0f - 1.0f;

        // 速度用乱数
        float3 randomVelocity = rand3dTo3d(randomPos + 1.234f);
        randomVelocity = randomVelocity * 2.0f - 1.0f;

        // 色用乱数
        float3 randomColor = rand3dTo3d(randomVelocity + 2.345f);

        // scale用乱数
        float randomScale = 0.1f + rand1dTo1d(localSeed.x + 3.456f) * 0.4f;

        // lifeTime用乱数
        float randomLifeTime = 0.5f + rand1dTo1d(localSeed.y + 4.567f) * 1.5f;

        // 発生位置を半径で広げる
        float3 spawnOffset = randomPos * gEmitter.radius;

        // 上方向に少し持ち上げる
        randomVelocity.y = abs(randomVelocity.y) + 0.5f;

        // Particleを初期化する
        gParticles[particleIndex].translate = gEmitter.translate + spawnOffset;
        gParticles[particleIndex].scale = float3(randomScale, randomScale, randomScale);
        gParticles[particleIndex].lifeTime = randomLifeTime;
        gParticles[particleIndex].velocity = randomVelocity;
        gParticles[particleIndex].currentTime = 0.0f;
        gParticles[particleIndex].color = float4(randomColor, 1.0f);
    }
}
