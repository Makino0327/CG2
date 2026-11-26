#include "../../project/game/player/Gun.h"
#include "../../project/game/player/PlayerBullet.h"
#include "../../project/game/collision/IBulletTarget.h"
#include "../../project/game/enemy/Enemy.h"
#include "../../project/game/boss/Boss.h"
#include "../../project/engine/particle/IParticleMesh.h"
#include "../../project/engine/particle/Ring.h"
#include "../../project/engine/particle/Cylinder.h"
#include "../../project/engine/base/offscreen/PostEffect.h"
#include "../../project/scene/result/ResultScene.h"
#include "../../project/scene/clear/ClearScene.h"
#include "../../project/scene/gameover/GameOverScene.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <limits>
#include <type_traits>
#include <vector>

// 基底型から破棄しても派生側のリソースが解放されることを型の契約として確認する。
static_assert(std::is_abstract_v<Gun> && std::has_virtual_destructor_v<Gun>);
static_assert(std::is_abstract_v<IParticleMesh> && std::has_virtual_destructor_v<IParticleMesh>);
static_assert(std::is_abstract_v<IPostEffect> && std::has_virtual_destructor_v<IPostEffect>);
static_assert(std::is_abstract_v<IBulletTarget> && std::has_virtual_destructor_v<IBulletTarget>);
static_assert(std::is_abstract_v<ResultScene> && std::has_virtual_destructor_v<ResultScene>);
static_assert(std::is_base_of_v<IParticleMesh, Ring> && std::is_base_of_v<IParticleMesh, Cylinder>);
static_assert(std::is_base_of_v<IBulletTarget, Enemy> && std::is_base_of_v<IBulletTarget, Boss>);
static_assert(std::is_base_of_v<ResultScene, ClearScene> && !std::is_abstract_v<ClearScene>);
static_assert(std::is_base_of_v<ResultScene, GameOverScene> && !std::is_abstract_v<GameOverScene>);

// GPUを使わず、共通の対象選択が具体型に依存していないことを確認する。
class TimedTarget final : public IBulletTarget {
public:
    TimedTarget(float time, bool dead, bool hittable, int& destructions)
        : time_(time), dead_(dead), hittable_(hittable), destructions_(destructions) {}
    ~TimedTarget() override { ++destructions_; }
    bool IsDead() const override { return dead_; }
    bool CheckBulletHit(const PlayerBullet&, float& hitTime) const override {
        hitTime = time_;
        return hittable_;
    }
    BulletHitFeedback ApplyBulletHit(const PlayerBullet&, float) override {
        dead_ = true;
        return {Vector3{1,2,3}};
    }
private:
    float time_;
    bool dead_;
    bool hittable_;
    int& destructions_;
};

static void CheckHandgun() {
    std::unique_ptr<Gun> gun = std::make_unique<Handgun>();
    int shots = 0;
    bool canEmit = true;
    GunFireContext context;
    context.emitBullet = [&](float spread) { assert(spread == 0.0f); ++shots; return canEmit; };
    // 押しっぱなしでは増えず、生成できたクリックだけが一発を消費する。
    assert(!gun->TryFire({true,false},context));
    assert(gun->TryFire({true,true},context) && gun->GetAmmo() == 6);
    canEmit = false;
    assert(!gun->TryFire({true,true},context) && gun->GetAmmo() == 6);
    canEmit = true;
    gun->StartReload();
    for (int frame = 0; frame < 69; ++frame) { gun->UpdateReload(); }
    assert(gun->IsReloading() && gun->GetAmmo() == 6);
    assert(!gun->TryFire({true,true},context));
    gun->UpdateReload();
    assert(!gun->IsReloading() && gun->GetAmmo() == 7 && gun->GetReserveAmmo() == 9);
    for (int index = 0; index < 7; ++index) { assert(gun->TryFire({true,true},context)); }
    assert(gun->GetAmmo() == 0 && !gun->TryFire({true,true},context));
    assert(gun->GetReloadTimer() == 70 && shots == 9);
    // チュートリアル用の無限補給でも、装填時間は短縮しない。
    gun->SetUnlimitedReserve(true);
    for (int frame = 0; frame < 70; ++frame) { gun->UpdateReload(); }
    assert(gun->GetAmmo() == 7 && gun->GetReserveAmmo() == 9);
}

static void CheckAssaultRifle() {
    std::unique_ptr<Gun> gun = std::make_unique<AssaultRifle>();
    std::vector<float> spreads;
    GunFireContext context;
    context.emitBullet = [&](float spread) { spreads.push_back(spread); return true; };
    assert(!gun->TryFire({true,true},context) && !gun->IsReloading());
    gun->AddReserveAmmo(5);
    gun->StartReload();
    for (int frame = 0; frame < 109; ++frame) { gun->UpdateReload(); }
    assert(gun->GetAmmo() == 0 && gun->GetReloadTimer() == 1);
    gun->UpdateReload();
    assert(gun->GetAmmo() == 5 && gun->GetReserveAmmo() == 0);
    // 連射は従来と同じ六フレーム間隔で、反動も成功回数に応じて増える。
    for (int frame = 0; frame < 13; ++frame) { gun->TryFire({true,frame == 0},context); }
    assert(spreads.size() == 3 && gun->GetAmmo() == 2);
    assert(spreads[0] == 0.0f && std::abs(spreads[1]-0.008f) < 0.00001f);
    assert(std::abs(spreads[2]-0.016f) < 0.00001f);
    gun->ResetTrigger();
    assert(gun->TryFire({true,true},context) && spreads.back() == 0.0f);
    gun->AddReserveAmmo(-10);
    assert(gun->GetReserveAmmo() == 0);
}

static void CheckShotgun() {
    std::unique_ptr<Gun> gun = std::make_unique<Shotgun>();
    gun->AddReserveAmmo(1);
    gun->StartReload();
    for (int frame = 0; frame < 95; ++frame) { gun->UpdateReload(); }
    int scatterShots = 0;
    GunFireContext context;
    context.emitBullet = [](float) { assert(false); return false; };
    context.emitScatter = [&](const ScatterShot& pattern) {
        assert(pattern.pelletCount == 5 && pattern.spreadAngle == 0.34f);
        ++scatterShots;
        return true;
    };
    // 五発の散弾でも消費は一発で、押しっぱなしによる追加発射は起こらない。
    assert(!gun->TryFire({true,false},context));
    assert(gun->TryFire({true,true},context));
    assert(scatterShots == 1 && gun->GetAmmo() == 0 && gun->GetReserveAmmo() == 0);
    assert(!gun->TryFire({true,true},context) && scatterShots == 1);
}

static void CheckNearestTarget() {
    int destructions = 0;
    {
        PlayerBullet bullet;
        std::unique_ptr<IBulletTarget> farTarget = std::make_unique<TimedTarget>(0.9f,false,true,destructions);
        std::unique_ptr<IBulletTarget> nearTarget = std::make_unique<TimedTarget>(0.2f,false,true,destructions);
        std::unique_ptr<IBulletTarget> tie = std::make_unique<TimedTarget>(0.2f,false,true,destructions);
        std::unique_ptr<IBulletTarget> dead = std::make_unique<TimedTarget>(0.1f,true,true,destructions);
        std::unique_ptr<IBulletTarget> miss = std::make_unique<TimedTarget>(0.01f,false,false,destructions);
        // 対象の実装が不正な時刻を返しても、移動区間内の命中だけを採用する。
        std::unique_ptr<IBulletTarget> invalidBefore = std::make_unique<TimedTarget>(-0.1f,false,true,destructions);
        std::unique_ptr<IBulletTarget> invalidAfter = std::make_unique<TimedTarget>(2.0f,false,true,destructions);
        std::unique_ptr<IBulletTarget> invalidNan = std::make_unique<TimedTarget>(
            std::numeric_limits<float>::quiet_NaN(),false,true,destructions);
        BulletTargetQuery query(bullet);
        for (auto* target : {farTarget.get(),nearTarget.get(),tie.get(),dead.get(),miss.get(),
            invalidBefore.get(),invalidAfter.get(),invalidNan.get()}) { query.Consider(*target); }
        assert(query.GetTarget() == nearTarget.get() && query.GetHitTime() == 0.2f);
        const auto feedback = query.GetTarget()->ApplyBulletHit(bullet,query.GetHitTime());
        assert(nearTarget->IsDead() && feedback.defeatFocus && feedback.defeatFocus->y == 2.0f);
    }
    assert(destructions == 8);
}

int main() {
    CheckHandgun();
    CheckAssaultRifle();
    CheckShotgun();
    CheckNearestTarget();
    std::cout << "PASS: polymorphic guns, reload timing, scatter ammo, nearest target and virtual destruction\n";
}
