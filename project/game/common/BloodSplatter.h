#pragma once

#include "../../engine/particle/Particle.h"
#include <random>

namespace HitEffects {
    // 敵とプレイヤーで、発光しない暗赤色の血しぶきの見た目を統一する。
    inline void EmitBloodSplatter(
        ParticleSystem* bloodParticleSystem,
        const Vector3& hitPosition,
        const Vector3& hitDirection)
    {
        if (!bloodParticleSystem) {
            return;
        }

        // 攻撃の進行方向とは逆側へ血しぶきを噴き出させる
        Vector3 reverseHitDirection = {
            -hitDirection.x,
            -hitDirection.y,
            -hitDirection.z
        };
        Vector3 forward = Normalize(reverseHitDirection);
        Vector3 side = { -forward.z, 0.0f, forward.x };
        // 攻撃を受けた体の表面から血しぶきを出す
        Vector3 bloodPosition = hitPosition;

        // 被弾方向を中心に密度の高い扇状の血しぶきを作る
        constexpr int kBloodParticleCount = 22;

        // 粒子が同じ位置に重ならないよう発生位置をランダムに散らす
        static std::mt19937 randomEngine(std::random_device{}());
        std::uniform_real_distribution<float> sideOffsetDistribution(-0.38f, 0.38f);
        std::uniform_real_distribution<float> heightOffsetDistribution(-0.28f, 0.38f);
        std::uniform_real_distribution<float> depthOffsetDistribution(-0.12f, 0.22f);
        std::uniform_real_distribution<float> sizeDistribution(0.55f, 1.0f);

        for (int index = 0; index < kBloodParticleCount; ++index) {
            float ratio = static_cast<float>(index) /
                static_cast<float>(kBloodParticleCount - 1);

            // -1.0から1.0へ変化させて左右へ広げる
            float sideRatio = ratio * 2.0f - 1.0f;
            float forwardPower = 1.2f + 0.18f * static_cast<float>(index % 6);
            float sidePower = sideRatio * (0.65f + 0.12f * static_cast<float>(index % 5));
            float upwardPower = 0.25f + 0.16f * static_cast<float>(index % 7);

            Vector3 velocity = {
                forward.x * forwardPower + side.x * sidePower,
                upwardPower,
                forward.z * forwardPower + side.z * sidePower
            };

            float size = sizeDistribution(randomEngine);
            float lifeTime = 0.28f + 0.045f * static_cast<float>(index % 6);

            float sideOffset = sideOffsetDistribution(randomEngine);
            float heightOffset = heightOffsetDistribution(randomEngine);
            float depthOffset = depthOffsetDistribution(randomEngine);

            Vector3 spawnPosition = {
                bloodPosition.x + side.x * sideOffset + forward.x * depthOffset,
                bloodPosition.y + heightOffset,
                bloodPosition.z + side.z * sideOffset + forward.z * depthOffset
            };

            Vector4 color{};
            if ((index % 3) == 0) {
                color = { 0.30f, 0.008f, 0.006f, 0.55f };
            } else if ((index % 3) == 1) {
                color = { 0.52f, 0.014f, 0.008f, 0.50f };
            } else {
                color = { 0.68f, 0.025f, 0.012f, 0.45f };
            }

            bloodParticleSystem->Emit(
                spawnPosition,
                { size, size, size },
                velocity,
                color,
                lifeTime);
        }

        // 命中箇所へ大きめの濃い血の塊を重ねる
        bloodParticleSystem->Emit(
            bloodPosition,
            { 1.1f, 1.1f, 1.1f },
            { forward.x * 0.7f, 0.3f, forward.z * 0.7f },
            { 0.38f, 0.008f, 0.005f, 0.48f },
            0.22f);
    }
}

