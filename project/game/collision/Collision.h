#pragma once

#include "../../engine/math/Math.h"

// 球の当たり判定に必要な情報をまとめる
struct SphereCollider
{
    Vector3 center;
    float radius;
};

class Collision
{
public:
    // 2つの球が当たっているかを判定する
    static bool IsHit(const SphereCollider& a, const SphereCollider& b)
    {
        // 2点間の差分を求める
        const float dx = a.center.x - b.center.x;
        const float dy = a.center.y - b.center.y;
        const float dz = a.center.z - b.center.z;

        // 中心間距離の2乗を求める
        const float distanceSq = dx * dx + dy * dy + dz * dz;

        // 半径の合計を求める
        const float radiusSum = a.radius + b.radius;

        // 中心間距離が半径の合計以内なら当たっている
        return distanceSq <= radiusSum * radiusSum;
    }
};
