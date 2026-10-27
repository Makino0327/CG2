#include "../../project/game/collision/BodyContact.h"
#include <cassert>
#include <iostream>

int main()
{
    // 通常の重なりと、中心が完全に一致する場合のゼロ除算を確認する。
    for (float enemyX : { 0.0f, 1.5f }) {
        SphereCollider player{ { 0.0f, 1.0f, 0.0f }, 1.0f };
        SphereCollider enemy{ { enemyX, 1.0f, 0.0f }, 1.0f };
        assert(BodyContact::Separate(player, enemy, nullptr, nullptr));
        assert(!Collision::IsHit(player, enemy));
        assert(std::isfinite(player.center.x) && std::isfinite(enemy.center.x));
        assert(player.center.y == 1.0f && enemy.center.y == 1.0f);
    }

    // 接触していない相手は動かさず、別の高さにいる相手も押さない。
    SphereCollider player{ { 0.0f, 1.0f, 0.0f }, 1.0f };
    SphereCollider distant{ { 0.0f, 5.0f, 0.0f }, 1.0f };
    assert(!BodyContact::Separate(player, distant, nullptr, nullptr));
    assert(player.center.x == 0.0f && distant.center.y == 5.0f);

    const std::vector<LevelColliderData> walls{
        { "BOX", { 3.0f, 1.0f, 0.0f }, { 0.2f, 4.0f, 20.0f }, true }
    };
    // 大きなノックバックでも薄い壁を抜けず、斜め方向は壁に沿って進む。
    const Vector3 stopped = BodyContact::Move({ 0.0f, 1.0f, 0.0f },
        { 8.0f, 0.0f, 2.0f }, 1.0f, &walls, nullptr);
    assert(std::abs(stopped.x - 1.9f) < 0.0001f);
    assert(std::abs(stopped.z - 2.0f) < 0.0001f);

    // プレイヤーの背後が壁でも、敵側へ押し戻しを渡して重なりを解消する。
    player = { { 1.9f, 1.0f, 0.0f }, 1.0f };
    SphereCollider enemy{ { 0.9f, 1.0f, 0.0f }, 1.0f };
    BodyContact::Separate(player, enemy, &walls, nullptr);
    assert(player.center.x <= 1.9001f);
    assert(!Collision::IsHit(player, enemy));

    // 敵の背後が壁の場合も、プレイヤー側が残りの押し戻しを受け持つ。
    player = { { 0.9f, 1.0f, 0.0f }, 1.0f };
    enemy = { { 1.9f, 1.0f, 0.0f }, 1.0f };
    BodyContact::Separate(player, enemy, &walls, nullptr);
    assert(enemy.center.x <= 1.9001f);
    assert(!Collision::IsHit(player, enemy));

    // 左右から挟まれても、両者へ分配して同じ二点を往復し続けない。
    player = { { 0.0f, 1.0f, 0.0f }, 1.0f };
    SphereCollider enemies[]{
        { { -1.2f, 1.0f, 0.0f }, 1.0f }, { { 1.2f, 1.0f, 0.0f }, 1.0f }
    };
    for (int pass = 0; pass < 4; ++pass) {
        for (auto& body : enemies) { BodyContact::Separate(player, body, nullptr, nullptr); }
    }
    for (const auto& body : enemies) { assert(!Collision::IsHit(player, body)); }

    // 移動先の床上面に足元を合わせ、高さの違う壁には引っかからない。
    const std::vector<LevelColliderData> floors{
        { "BOX", { 0.0f, -0.5f, 0.0f }, { 20.0f, 1.0f, 20.0f }, true }
    };
    const std::vector<LevelColliderData> highWalls{
        { "BOX", { 3.0f, 20.0f, 0.0f }, { 0.2f, 2.0f, 20.0f }, true }
    };
    const Vector3 grounded = BodyContact::Move({ 0.0f, 1.0f, 0.0f },
        { 5.0f, -10.0f, 0.0f }, 1.0f, &highWalls, &floors);
    assert(std::abs(grounded.x - 5.0f) < 0.0001f && grounded.y == 1.0f);
    std::cout << "Body contact tests passed.\n";
}
