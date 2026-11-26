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
        state_->OnBegin(*this);
        return true;
    }

    void Update()
    {
        moved_ = false;
        previous_ = position_;
        // 予告・走行・完了の振る舞いは、保持している状態へ任せる。
        state_->Update(*this);
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

    bool IsWindingUp() const { return state_->GetPhase() == Phase::Windup; }
    bool IsRunning() const { return state_->GetPhase() == Phase::Running; }
    bool IsFinished() const { return state_->GetPhase() == Phase::Finished; }
    float GetWindupProgress() const { return static_cast<float>(windupAge_) / kWindupFrames; }
    const Vector3& GetStart() const { return start_; }
    const Vector3& GetEnd() const { return end_; }
    const Vector3& GetDirection() const { return direction_; }
    const Vector3& GetPosition() const { return position_; }

private:
    enum class Phase { Inactive, Windup, Running, Finished };
    class ChargeState {
    public:
        virtual ~ChargeState() = default;
        virtual Phase GetPhase() const = 0;
        virtual void Update(BossCharge&) const {}
        virtual void OnBegin(BossCharge&) const {}
    };
    class InactiveState final : public ChargeState {
    public:
        Phase GetPhase() const override { return Phase::Inactive; }
        void OnBegin(BossCharge& owner) const override { owner.ChangeState<WindupState>(); }
    };
    class WindupState final : public ChargeState {
    public:
        Phase GetPhase() const override { return Phase::Windup; }
        void Update(BossCharge& owner) const override {
            // 最後の予告フレームは動かず、次のフレームから走る。
            if (++owner.windupAge_ >= kWindupFrames) { owner.ChangeState<RunningState>(); }
        }
    };
    class RunningState final : public ChargeState {
    public:
        Phase GetPhase() const override { return Phase::Running; }
        void Update(BossCharge& owner) const override {
            const float remaining = std::hypot(owner.end_.x - owner.position_.x, owner.end_.z - owner.position_.z);
            const float step = (std::min)(kSpeed, remaining);
            owner.position_.x += owner.direction_.x * step;
            owner.position_.z += owner.direction_.z * step;
            owner.moved_ = step > 0.0f;
            if (remaining <= kSpeed) {
                owner.position_ = owner.end_;
                owner.ChangeState<FinishedState>();
            }
        }
    };
    class FinishedState final : public ChargeState {
    public:
        Phase GetPhase() const override { return Phase::Finished; }
    };
    template<class T> static const ChargeState& GetState() { static const T state; return state; }
    template<class T> void ChangeState() { state_ = &GetState<T>(); }
    const ChargeState* state_ = &GetState<InactiveState>();
    int windupAge_ = 0;
    float radius_ = 0.0f;
    Vector3 start_{}, end_{}, direction_{}, position_{}, previous_{};
    bool moved_ = false;
    bool hitPlayer_ = false;
};
