#pragma once

#include <algorithm>
#include <cmath>
#include "../collision/Collision.h"
#include "../../scene/LevelLoader.h"

// 突進の予告・移動・命中を同じ経路で管理し、描画なしでも検証できるようにする。
class BossCharge
{
public:
    static constexpr int kWindupFrames = 60; // 60FPSで1秒間、走る範囲を予告する。
    static constexpr float kSpeed = 0.65f; // プレイヤーより速く直進する1フレームの距離。

    void Reset() { *this = BossCharge{}; }
    void BeginFrame() { moved_ = false; } // 硬直中へ、直前の移動判定を持ち越さない。

    bool Begin(const Vector3& start, const Vector3& target, float radius,
        const std::vector<LevelColliderData>* walls)
    {
        Reset();
        const float dx = target.x - start.x, dz = target.z - start.z;
        const float distance = std::hypot(dx, dz);
        if (distance < 0.001f) { return false; }
        start_ = position_ = previous_ = start;
        direction_ = { dx / distance, 0.0f, dz / distance };
        radius_ = radius;
        // 狙った位置を少し通り越すが、溜め始めた後は方向を変えない。
        const float length = (std::min)(distance + 2.0f, 40.0f);
        end_ = { start.x + direction_.x * length, start.y, start.z + direction_.z * length };
        float nearest = 1.0f;
        if (walls) {
            for (const auto& wall : *walls) {
                if (!wall.hasCollider || wall.type != "BOX" ||
                    std::abs(start.y - wall.center.y) >= wall.size.y * 0.5f + radius) { continue; }
                // 巨体の幅で壁を広げ、薄い壁も飛び越えずに手前で止まる。
                const Vector3 minimum{ wall.center.x - wall.size.x * 0.5f - radius,
                    start.y - 1.0f, wall.center.z - wall.size.z * 0.5f - radius };
                const Vector3 maximum{ wall.center.x + wall.size.x * 0.5f + radius,
                    start.y + 1.0f, wall.center.z + wall.size.z * 0.5f + radius };
                float hitTime = 0.0f;
                if (Collision::SweepSphereAabb(start, end_, 0.0f, minimum, maximum, hitTime)) {
                    // 既に壁へ接していて、壁から離れる方向への突進は妨げない。
                    const Vector3 probe{ start.x + direction_.x * 0.01f, start.y,
                        start.z + direction_.z * 0.01f };
                    const bool leaving = probe.x < minimum.x || probe.x > maximum.x ||
                        probe.z < minimum.z || probe.z > maximum.z;
                    if (hitTime > 0.0f || !leaving) { nearest = (std::min)(nearest, hitTime); }
                }
            }
        }
        // 接触面から少し手前へ戻し、丸め誤差で壁へ埋まらないようにする。
        const float travel = (std::max)(0.0f, length * nearest - (nearest < 1.0f ? 0.05f : 0.0f));
        end_ = { start.x + direction_.x * travel, start.y, start.z + direction_.z * travel };
        if (travel < 0.1f) { return false; }
        phase_ = Phase::Windup;
        return true;
    }

    void Update()
    {
        moved_ = false;
        previous_ = position_;
        if (phase_ == Phase::Windup) {
            // 最後の予告フレームでは移動せず、次のフレームから走り出す。
            if (++windupAge_ >= kWindupFrames) { phase_ = Phase::Running; }
        } else if (phase_ == Phase::Running) {
            const float remaining = std::hypot(end_.x - position_.x, end_.z - position_.z);
            const float step = (std::min)(kSpeed, remaining);
            position_.x += direction_.x * step;
            position_.z += direction_.z * step;
            moved_ = step > 0.0f;
            if (remaining <= kSpeed) { position_ = end_; phase_ = Phase::Finished; }
        }
    }

    bool TryHit(const SphereCollider& target)
    {
        if (!moved_ || hitPlayer_ || std::abs(target.center.y - position_.y) > radius_ + target.radius) {
            return false;
        }
        // 高速移動の途中もXZ平面で調べ、通過したプレイヤーをすり抜けない。
        SphereCollider horizontalTarget = target;
        horizontalTarget.center.y = position_.y;
        float hitTime = 0.0f;
        if (!Collision::SweepSphere(previous_, position_, radius_, horizontalTarget, hitTime)) { return false; }
        hitPlayer_ = true; // 1回の突進で何度もダメージを与えない。
        return true;
    }

    bool IsWindingUp() const { return phase_ == Phase::Windup; }
    bool IsRunning() const { return phase_ == Phase::Running; }
    bool IsFinished() const { return phase_ == Phase::Finished; }
    float GetWindupProgress() const { return static_cast<float>(windupAge_) / kWindupFrames; }
    const Vector3& GetStart() const { return start_; }
    const Vector3& GetEnd() const { return end_; }
    const Vector3& GetDirection() const { return direction_; }
    const Vector3& GetPosition() const { return position_; }

private:
    enum class Phase { Inactive, Windup, Running, Finished };
    Phase phase_ = Phase::Inactive;
    int windupAge_ = 0;
    float radius_ = 0.0f;
    Vector3 start_{}, end_{}, direction_{}, position_{}, previous_{};
    bool moved_ = false;
    bool hitPlayer_ = false;
};
