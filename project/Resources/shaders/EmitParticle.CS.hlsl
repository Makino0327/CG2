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

            // 中心から外側へ飛ばすための方向を作る
            float3 randomDirection = rand3dTo3d(localSeed) * 2.0f - 1.0f;
            if (length(randomDirection) < 0.001f)
            {
                randomDirection = float3(0.0f, 1.0f, 0.0f);
            }
            randomDirection = normalize(randomDirection);

            // 発生位置は中心の近くに小さく散らす
            float spawnRadius = rand1dTo1d(localSeed.x + 3.456f) * gEmitter.radius;
            float3 spawnOffset = randomDirection * spawnRadius;

            // 外側へ広がる速度に少しだけ強弱を付ける
            float speed = 1.2f + rand1dTo1d(localSeed.y + 4.567f) * 1.8f;
            float3 randomVelocity = randomDirection * speed;

            // 少しだけ上に浮くようにする
            randomVelocity.y += 0.25f;

            // サイズと寿命は控えめにする
            float randomScale = 0.06f + rand1dTo1d(localSeed.z + 5.678f) * 0.12f;
            float randomLifeTime = 1.0f + rand1dTo1d(localSeed.y + 6.789f) * 1.0f;

            // 中心から外へ出るのが見やすい暖色にする
            float3 randomColor = lerp(
                float3(1.0f, 0.45f, 0.12f),
                float3(1.0f, 0.95f, 0.35f),
                rand1dTo1d(localSeed.x + 7.890f));

            // Particle を初期化する
            gParticles[particleIndex].translate = gEmitter.translate + spawnOffset;
            gParticles[particleIndex].scale = float3(randomScale, randomScale, randomScale);
            gParticles[particleIndex].lifeTime = randomLifeTime;
            gParticles[particleIndex].velocity = randomVelocity;
            gParticles[particleIndex].currentTime = 0.0f;
            gParticles[particleIndex].color = float4(randomColor, 0.9f);
        }
        else
        {
            // 空きが無かったのでIndexを戻してこのフレームのEmitを打ち切る
            InterlockedAdd(gFreeListIndex[0], 1);
            break;
        }
    }
}
