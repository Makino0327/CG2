#include "../../project/game/boss/BossCharge.h"
#include <cassert>
#include <iostream>

// 予告時間・回避・壁・終点の命中を、ゲームと同じ突進処理で確認する。
int main()
{
    BossCharge charge;
    assert(charge.Begin({ 0, 3, 0 }, { 0, 1, 20 }, 3.3f, nullptr));
    const Vector3 end = charge.GetEnd();
    assert(std::abs(end.z - 22.0f) < 0.001f);
    for (int frame = 0; frame < 60; ++frame) {
        assert(charge.IsWindingUp());
        assert(!charge.TryHit({ { 0, 1, 0 }, 1 }));
        charge.Update();
        assert(charge.GetPosition().z == 0.0f);
    }
    assert(charge.IsRunning());
    charge.Update();
    assert(std::abs(charge.GetPosition().z - BossCharge::kSpeed) < 0.001f);
    // 赤い帯の外へ横に避けた相手には当たらない。
    assert(!charge.TryHit({ { 5, 1, 0 }, 1 }));
    assert(charge.TryHit({ { 0, 1, 0.3f }, 1 }));
    assert(!charge.TryHit({ { 0, 1, 0.3f }, 1 }));
    while (!charge.IsFinished()) { charge.Update(); }
    assert(charge.GetPosition().z == end.z);
    charge.Reset();
    assert(!charge.IsRunning() && !charge.IsWindingUp());
    assert(!charge.TryHit({ { 0, 1, 22 }, 1 }));

    // 進行方向の薄い壁より、ボスの半径ぶん手前で停止する。
    const std::vector<LevelColliderData> walls{ { "BOX", { 0, 1, 10 }, { 20, 2, 0.2f }, true } };
    assert(charge.Begin({ 0, 3, 0 }, { 0, 1, 20 }, 3.3f, &walls));
    assert(charge.GetEnd().z < 6.6f && charge.GetEnd().z > 6.4f);
    for (int frame = 0; frame < 60; ++frame) { charge.Update(); }
    while (!charge.IsFinished()) { charge.Update(); }
    assert(charge.GetPosition().z < 6.6f);

    // 狭い当たり判定でも、1フレームの途中で通過した相手を拾う。
    assert(charge.Begin({ 0, 0, 0 }, { 0, 0, 3 }, 0.05f, nullptr));
    for (int frame = 0; frame < 60; ++frame) { charge.Update(); }
    charge.Update();
    assert(charge.TryHit({ { 0, 0, 0.3f }, 0.05f }));
    // 最後の移動フレームは命中し、次の硬直フレームには持ち越さない。
    assert(charge.Begin({ 0, 0, 0 }, { 0, 0, 3 }, 0.05f, nullptr));
    for (int frame = 0; frame < 60; ++frame) { charge.Update(); }
    while (!charge.IsFinished()) { charge.Update(); }
    assert(charge.TryHit({ charge.GetEnd(), 0.05f }));
    charge.BeginFrame();
    assert(!charge.TryHit({ charge.GetEnd(), 0.05f }));
    assert(!charge.Begin({ 0, 0, 0 }, { 0, 0, 0 }, 1, nullptr));
    std::cout << "Boss charge tests passed\n";
}
