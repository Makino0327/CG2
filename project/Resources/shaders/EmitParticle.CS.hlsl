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
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<int> gFreeList : register(u2);

// 0.0fから1.0fの乱数を返す
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

// float3 seedから0.0fから1.0fの乱数float3を作る
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

    // count個のParticleを発生させる
    for (uint countIndex = 0; countIndex < gEmitter.count; ++countIndex)
    {
        int freeListIndex;

        // FreeListの先頭を1つ消費して現在のIndexを取得する
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        // FreeListに有効な空きがあるときだけ使う
        if (0 <= freeListIndex && freeListIndex < (int) kMaxParticles)
        {
            uint particleIndex = gFreeList[freeListIndex];

            // countIndexごとに少し別のseedになるようにずらす
            float3 localSeed = seed + float3(
                (float) countIndex * 0.11f,
                (float) countIndex * 0.17f,
                (float) countIndex * 0.23f
            );

            // 位置用乱数
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

            // 発生位置を半径内でずらす
            float3 spawnOffset = randomPos * gEmitter.radius;

            // 少し上方向に飛びやすくする
            randomVelocity.y = abs(randomVelocity.y) + 0.5f;

            // Particleを初期化する
            gParticles[particleIndex].translate = gEmitter.translate + spawnOffset;
            gParticles[particleIndex].scale = float3(randomScale, randomScale, randomScale);
            gParticles[particleIndex].lifeTime = randomLifeTime;
            gParticles[particleIndex].velocity = randomVelocity;
            gParticles[particleIndex].currentTime = 0.0f;
            gParticles[particleIndex].color = float4(randomColor, 1.0f);
        }
        else
        {
            // 空きが無かったのでIndexを戻してこのフレームのEmitを打ち切る
            InterlockedAdd(gFreeListIndex[0], 1);
            break;
        }
    }
}
