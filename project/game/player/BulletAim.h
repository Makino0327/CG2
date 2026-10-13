#pragma once
#include "../../engine/math/Math.h"
#include "../collision/Collision.h"
#include <cmath>
#include <vector>

// マウスの線または弾の射線が最初に触れる敵の表面と、胴体の中心を返す。
inline bool FindBulletAimTarget(const Vector3& rayStart, const Vector3& rayEnd,
    const std::vector<BoxCollider>& boxes, const std::vector<SphereCollider>& spheres,
    Vector3& target, float& nearestTime, Vector3* bodyCenter = nullptr, float radius = 0.0f)
{
    nearestTime = 1.0f;
    bool found = false;
    Vector3 selectedCenter{};
    for (const BoxCollider& box : boxes) {
        float time = 0.0f;
        if (Collision::SweepSphereBox(rayStart, rayEnd, radius, box, time) && time <= nearestTime) {
            nearestTime = time;
            selectedCenter = box.center;
            found = true;
        }
    }
    for (const SphereCollider& sphere : spheres) {
        float time = 0.0f;
        if (Collision::SweepSphere(rayStart, rayEnd, radius, sphere, time) && time <= nearestTime) {
            nearestTime = time;
            selectedCenter = sphere.center;
            found = true;
        }
    }
    if (found) {
        if (bodyCenter) { *bodyCenter = selectedCenter; }
        target = { rayStart.x+(rayEnd.x-rayStart.x)*nearestTime,
            rayStart.y+(rayEnd.y-rayStart.y)*nearestTime,
            rayStart.z+(rayEnd.z-rayStart.z)*nearestTime };
    }
    return found;
}

// 敵を直接狙っている場合は、敵が銃口より手前でも狙い位置を遠ざけない。
inline Vector3 CalculateDirectBulletAimDirection(const Vector3& muzzle, const Vector3& target)
{
    const Vector3 direction{ target.x-muzzle.x, target.y-muzzle.y, target.z-muzzle.z };
    const float length = std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    if (length <= 0.0001f) { return { 0, 0, 0 }; }
    return { direction.x/length, direction.y/length, direction.z/length };
}

// 狙う高さへ投影した目標を使い、近い敵の上へ弾が通り抜けない方向を求める。
inline Vector3 CalculateBulletAimDirection(const Vector3& muzzle, const Vector3& player,
    Vector3 target, float targetHeight)
{
    const float dx = target.x-player.x, dz = target.z-player.z;
    const float targetDistance = std::sqrt(dx*dx+dz*dz);
    if (targetDistance <= 0.0001f) { return { 0, 0, 0 }; }
    const float muzzleX = muzzle.x-player.x, muzzleZ = muzzle.z-player.z;
    const float muzzleDistance = std::sqrt(muzzleX*muzzleX+muzzleZ*muzzleZ);
    // 目標を大きく遠ざけず、銃口より後ろへ撃たないための最小限の余白だけ取る。
    constexpr float kMuzzleClearance = 0.10f;
    const float minimumDistance = muzzleDistance+kMuzzleClearance;
    if (targetDistance < minimumDistance) {
        target.x = player.x+dx/targetDistance*minimumDistance;
        target.z = player.z+dz/targetDistance*minimumDistance;
    }
    const Vector3 direction{ target.x-muzzle.x, targetHeight-muzzle.y, target.z-muzzle.z };
    const float length = std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    if (length <= 0.0001f) { return { 0, 0, 0 }; }
    return { direction.x/length, direction.y/length, direction.z/length };
}
