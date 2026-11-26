#pragma once
#include <algorithm>
#include <functional>
#include <limits>

// 散弾の見た目はデータで渡し、弾の生成資源を銃に所有させない。
struct ScatterShot {
    int pelletCount = 5;
    float spreadAngle = 0.34f;
    float randomSpreadAngle = 0.08f;
    float minSpeedScale = 0.85f;
    float maxSpeedScale = 1.08f;
};
struct GunTrigger { bool pressed; bool triggered; };
struct GunFireContext {
    std::function<bool(float)> emitBullet;
    std::function<bool(const ScatterShot&)> emitScatter;
};

// 弾薬とリロードの規則は共通化し、射撃方法だけを派生クラスで切り替える。
class Gun {
public:
    virtual ~Gun() = default;
    virtual bool TryFire(const GunTrigger& trigger, const GunFireContext& context) = 0;
    // 単発の銃は継続入力の状態を持たないため、既定の解除処理は不要。
    virtual void ResetTrigger() {}
    int GetAmmo() const { return ammo_; }
    int GetMaxAmmo() const { return capacity_; }
    int GetReserveAmmo() const { return reserve_; }
    int GetReloadTimer() const { return reloadTimer_; }
    bool IsReloading() const { return reloadTimer_ > 0; }
    void SetUnlimitedReserve(bool enabled) { unlimitedReserve_ = enabled; }
    void AddReserveAmmo(int rounds) {
        // 不正な補給と整数の上限超過を防ぎ、残弾を非負に保つ。
        if (rounds > 0) { reserve_ += (std::min)(rounds, (std::numeric_limits<int>::max)()-reserve_); }
    }
    void StartReload() {
        if (!IsReloading() && ammo_ < capacity_ && (unlimitedReserve_ || reserve_ > 0)) {
            reloadTimer_ = reloadDuration_;
        }
    }
    void UpdateReload() {
        if (reloadTimer_ <= 0 || --reloadTimer_ > 0) { return; }
        const int rounds = unlimitedReserve_ ? capacity_-ammo_ : (std::min)(capacity_-ammo_,reserve_);
        ammo_ += rounds;
        if (!unlimitedReserve_) { reserve_ -= rounds; }
    }
    void CancelReload() { reloadTimer_ = 0; }
    void RefillMagazine() {
        // 自動展示の周回開始など、明示的な補充だけを受け付ける。
        ammo_ = capacity_;
        CancelReload();
        ResetTrigger();
    }
protected:
    Gun(int capacity, int ammo, int reserve, int reloadDuration)
        // 派生クラスが設定を追加しても、弾数と装填時間の不変条件を守る。
        : capacity_((std::max)(1,capacity)), ammo_(std::clamp(ammo,0,capacity_)),
          reserve_((std::max)(0,reserve)), reloadDuration_((std::max)(1,reloadDuration)) {}
    template<class Emit> bool Fire(Emit&& emit) {
        if (IsReloading()) { return false; }
        if (ammo_ == 0) { StartReload(); return false; }
        // 弾を生成できた射撃だけを消費し、散弾も一回として数える。
        if (!emit()) { return false; }
        --ammo_;
        return true;
    }
private:
    int capacity_;
    int ammo_;
    int reserve_;
    int reloadDuration_;
    int reloadTimer_ = 0;
    bool unlimitedReserve_ = false;
};

class Handgun final : public Gun {
public:
    Handgun() : Gun(7,7,10,70) {}
    // クリックの立ち上がりにだけ、通常弾を一発生成する。
    bool TryFire(const GunTrigger& trigger, const GunFireContext& context) override {
        return trigger.triggered && context.emitBullet && Fire([&] { return context.emitBullet(0.0f); });
    }
};

class AssaultRifle final : public Gun {
public:
    AssaultRifle() : Gun(30,0,0,110) {}
    bool TryFire(const GunTrigger& trigger, const GunFireContext& context) override {
        // 押し続けている間だけ連射し、成功した射撃ごとに反動を増やす。
        if (cooldown_ > 0) { --cooldown_; }
        bool fired = false;
        if (trigger.pressed && cooldown_ == 0 && context.emitBullet) {
            const float spread = (std::min)(0.08f, static_cast<float>(continuousShots_)*0.008f);
            fired = Fire([&] { return context.emitBullet(spread); });
            if (fired) { ++continuousShots_; cooldown_ = 6; }
        }
        if (!trigger.pressed) { continuousShots_ = 0; }
        return fired;
    }
    void ResetTrigger() override { cooldown_ = continuousShots_ = 0; }
private:
    int cooldown_ = 0;
    int continuousShots_ = 0;
};

class Shotgun final : public Gun {
public:
    Shotgun() : Gun(6,0,0,95) {}
    // 一度のクリックで散弾を生成し、弾薬は一発だけ消費する。
    bool TryFire(const GunTrigger& trigger, const GunFireContext& context) override {
        return trigger.triggered && context.emitScatter && Fire([&] { return context.emitScatter(pattern_); });
    }
private:
    ScatterShot pattern_{};
};
