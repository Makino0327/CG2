#pragma once
#include <optional>
#include <cmath>
#include "../../engine/math/Math.h"
class PlayerBullet;

// 命中の結果だけを通知し、敵の具体型や内部のHPを外部へ公開しない。
struct BulletHitFeedback { std::optional<Vector3> defeatFocus; };

// 箱・球などの形状によらず、弾が当たる対象として扱うための最小の契約。
class IBulletTarget {
public:
    virtual ~IBulletTarget() = default;
    virtual bool IsDead() const = 0;
    virtual bool CheckBulletHit(const PlayerBullet& bullet, float& hitTime) const = 0;
    virtual BulletHitFeedback ApplyBulletHit(const PlayerBullet& bullet, float hitTime) = 0;
};

// 所有配列の種類に依存せず、一つの弾が最初に当たる対象を選ぶ。
class BulletTargetQuery {
public:
    explicit BulletTargetQuery(const PlayerBullet& bullet) : bullet_(bullet) {}
    void Consider(IBulletTarget& candidate) {
        if (candidate.IsDead()) { return; }
        float time = 0.0f;
        // 異なる実装から受け取る値も検証し、移動区間外の命中を採用しない。
        if (candidate.CheckBulletHit(bullet_,time) && std::isfinite(time) && time >= 0.0f && time <= 1.0f &&
            (!target_ || time < hitTime_)) {
            target_ = &candidate;
            hitTime_ = time;
        }
    }
    IBulletTarget* GetTarget() const { return target_; }
    float GetHitTime() const { return hitTime_; }
private:
    const PlayerBullet& bullet_;
    IBulletTarget* target_ = nullptr;
    float hitTime_ = 1.0f;
};
