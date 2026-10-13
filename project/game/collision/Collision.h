#pragma once

#include "../../engine/math/Math.h"
#include <algorithm>
#include <array>
#include <cmath>

// 球の当たり判定に必要な情報をまとめる
struct SphereCollider
{
    Vector3 center;
    float radius;
};

// Y軸まわりに回転する敵の見た目に合わせた箱の当たり判定。
struct BoxCollider
{
    Vector3 center;
    Vector3 halfSize;
    float yaw;
};

class Collision
{
public:
    // 描画のMakeAffineMatrixと同じ回転を逆に適用し、箱のローカル座標へ戻す。
    static Vector3 ToBoxLocal(const Vector3& point, const BoxCollider& box)
    {
        const float c = std::cos(box.yaw), s = std::sin(box.yaw);
        const float x = point.x-box.center.x, z = point.z-box.center.z;
        return { x*c-z*s, point.y-box.center.y, x*s+z*c };
    }

    // 箱のローカル座標を、回転と移動を含めたワールド座標へ戻す。
    static Vector3 FromBoxLocal(const Vector3& point, const BoxCollider& box)
    {
        const float c = std::cos(box.yaw), s = std::sin(box.yaw);
        return { box.center.x+point.x*c+point.z*s, box.center.y+point.y,
            box.center.z-point.x*s+point.z*c };
    }

    // 命中した弾の中心から最も近い箱の表面へ、血しぶきの位置を合わせる。
    static Vector3 ClosestPointOnBox(const Vector3& point, const BoxCollider& box)
    {
        const Vector3 local = ToBoxLocal(point, box);
        return FromBoxLocal({ std::clamp(local.x, -box.halfSize.x, box.halfSize.x),
            std::clamp(local.y, -box.halfSize.y, box.halfSize.y),
            std::clamp(local.z, -box.halfSize.z, box.halfSize.z) }, box);
    }

    // 回転した箱へ弾が最初に触れる位置を調べ、立方体の角も命中させる。
    static bool SweepSphereBox(const Vector3& start, const Vector3& end,
        float radius, const BoxCollider& box, float& hitTime)
    {
        return SweepSphereAabb(ToBoxLocal(start, box), ToBoxLocal(end, box), radius,
            { -box.halfSize.x, -box.halfSize.y, -box.halfSize.z }, box.halfSize, hitTime);
    }

    // 敵の移動量を弾から差し引き、同じフレーム内で両者が触れた時刻を求める。
    static bool SweepMovingSphereBox(const Vector3& start, const Vector3& end,
        float radius, const BoxCollider& box, const Vector3& previousCenter, float& hitTime)
    {
        const Vector3 relativeStart{ start.x+box.center.x-previousCenter.x,
            start.y+box.center.y-previousCenter.y, start.z+box.center.z-previousCenter.z };
        return SweepSphereBox(relativeStart, end, radius, box, hitTime);
    }

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

    // 移動する球と対象の球が最初に触れる位置を、移動区間内の割合で返す。
    static bool SweepSphere(const Vector3& start, const Vector3& end,
        float radius, const SphereCollider& target, float& hitTime)
    {
        const Vector3 delta{ end.x-start.x, end.y-start.y, end.z-start.z };
        const Vector3 offset{ start.x-target.center.x, start.y-target.center.y, start.z-target.center.z };
        const float radiusSum = radius+target.radius;
        const float c = offset.x*offset.x+offset.y*offset.y+offset.z*offset.z-radiusSum*radiusSum;
        // 発射時点から重なっている場合も、移動前の位置で命中させる。
        if (c <= 0.0f) { hitTime = 0.0f; return true; }
        const float a = delta.x*delta.x+delta.y*delta.y+delta.z*delta.z;
        const float b = offset.x*delta.x+offset.y*delta.y+offset.z*delta.z;
        if (a <= 0.0f || b >= 0.0f) { return false; }
        const float discriminant = b*b-a*c;
        if (discriminant < 0.0f) { return false; }
        // 同じ符号の値の差を避け、遠くから撃つ場合の誤差を抑える。
        const float t = c/(-b+std::sqrt(discriminant));
        if (t > 1.0f) { return false; }
        hitTime = t;
        return true;
    }

    // 球の移動区間と壁の最短距離を調べ、薄い壁や角も飛び越さないようにする。
    static bool SweepSphereAabb(const Vector3& start, const Vector3& end,
        float radius, const Vector3& minimum, const Vector3& maximum, float& hitTime)
    {
        const std::array<float, 3> origin{ start.x, start.y, start.z };
        const std::array<float, 3> delta{ end.x-start.x, end.y-start.y, end.z-start.z };
        const std::array<float, 3> lower{ minimum.x, minimum.y, minimum.z };
        const std::array<float, 3> upper{ maximum.x, maximum.y, maximum.z };
        // マウスの線は半径0なので、各面との交差時刻から手前の面を直接求める。
        // 面に到達した座標を距離式へ戻す方法では、丸め誤差で奥の面を拾う場合がある。
        if (radius == 0.0f) {
            float enter = 0.0f, exit = 1.0f;
            for (size_t axis = 0; axis < 3; ++axis) {
                if (delta[axis] == 0.0f) {
                    if (origin[axis] < lower[axis] || origin[axis] > upper[axis]) { return false; }
                    continue;
                }
                const float a = (lower[axis]-origin[axis])/delta[axis];
                const float b = (upper[axis]-origin[axis])/delta[axis];
                enter = (std::max)(enter, (std::min)(a, b));
                exit = (std::min)(exit, (std::max)(a, b));
                if (enter > exit) { return false; }
            }
            hitTime = enter;
            return true;
        }
        // 移動区間から離れた壁は先に除外し、連射中の判定負荷を抑える。
        for (size_t axis = 0; axis < 3; ++axis) {
            if ((std::max)(origin[axis], origin[axis]+delta[axis])+radius < lower[axis] ||
                (std::min)(origin[axis], origin[axis]+delta[axis])-radius > upper[axis]) {
                return false;
            }
        }
        std::array<float, 8> times{ 0.0f, 1.0f };
        size_t count = 2;
        // 箱の各面を横切る時刻で分けると、各区間の最短距離を二次式で解ける。
        for (size_t axis = 0; axis < 3; ++axis) {
            if (delta[axis] == 0.0f) { continue; }
            for (float boundary : { lower[axis], upper[axis] }) {
                const float t = (boundary-origin[axis])/delta[axis];
                if (t > 0.0f && t < 1.0f) { times[count++] = t; }
            }
        }
        std::sort(times.begin(), times.begin()+count);
        for (size_t interval = 0; interval+1 < count; ++interval) {
            const float begin = times[interval];
            const float length = times[interval+1]-begin;
            const float middle = begin+length*0.5f;
            float a = 0.0f, b = 0.0f, c = -radius*radius;
            for (size_t axis = 0; axis < 3; ++axis) {
                const float point = origin[axis]+delta[axis]*begin;
                const float offset = point-std::clamp(point, lower[axis], upper[axis]);
                const float probe = origin[axis]+delta[axis]*middle;
                const float slope = probe < lower[axis] || probe > upper[axis] ? delta[axis] : 0.0f;
                a += slope*slope;
                b += offset*slope;
                c += offset*offset;
            }
            // 角の周囲も球の半径で判定し、箱を単純に広げた場合の誤判定を避ける。
            if (c <= 0.0f) { hitTime = begin; return true; }
            if (a <= 0.0f || b >= 0.0f) { continue; }
            const float discriminant = b*b-a*c;
            if (discriminant < 0.0f) { continue; }
            const float t = c/(-b+std::sqrt(discriminant));
            if (t <= length) { hitTime = begin+t; return true; }
        }
        return false;
    }
};
