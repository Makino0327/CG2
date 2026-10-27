#pragma once

#include "Collision.h"
#include "../../scene/LevelLoader.h"

namespace BodyContact {
    // 押し戻しを小分けにし、壁の手前で止めながら水平に移動する。
    inline Vector3 Move(Vector3 position, const Vector3& movement, float radius,
        const std::vector<LevelColliderData>* walls,
        const std::vector<LevelColliderData>* floors)
    {
        const float distance = std::sqrt(movement.x * movement.x + movement.z * movement.z);
        const int steps = (std::max)(1, static_cast<int>(std::ceil(distance / 0.1f)));
        const Vector3 step{ movement.x / steps, 0.0f, movement.z / steps };
        for (int i = 0; i < steps; ++i) {
            Vector3 next = position;
            next.x += step.x;
            if (walls) {
                for (const auto& wall : *walls) {
                    if (!wall.hasCollider || wall.type != "BOX" ||
                        std::abs(position.y - wall.center.y) >= wall.size.y * 0.5f + radius ||
                        std::abs(position.z - wall.center.z) >= wall.size.z * 0.5f + radius) {
                        continue;
                    }
                    // 壁を半径分だけ広げ、中心がその境界を越えないようにする。
                    const float left = wall.center.x - wall.size.x * 0.5f - radius;
                    const float right = wall.center.x + wall.size.x * 0.5f + radius;
                    if (step.x > 0.0f && position.x <= left && next.x > left) { next.x = left; }
                    if (step.x < 0.0f && position.x >= right && next.x < right) { next.x = right; }
                }
            }
            next.z += step.z;
            if (walls) {
                for (const auto& wall : *walls) {
                    if (!wall.hasCollider || wall.type != "BOX" ||
                        std::abs(position.y - wall.center.y) >= wall.size.y * 0.5f + radius ||
                        std::abs(next.x - wall.center.x) >= wall.size.x * 0.5f + radius) {
                        continue;
                    }
                    // 軸を分けて補正し、斜めに押された場合は壁に沿って滑らせる。
                    const float back = wall.center.z - wall.size.z * 0.5f - radius;
                    const float front = wall.center.z + wall.size.z * 0.5f + radius;
                    if (step.z > 0.0f && position.z <= back && next.z > back) { next.z = back; }
                    if (step.z < 0.0f && position.z >= front && next.z < front) { next.z = front; }
                }
            }
            position = next;
        }

        // 移動先の床上面へ足元を合わせ、水平な押し戻しで床へ沈めない。
        bool foundFloor = false;
        float groundY = 0.0f;
        if (floors) {
            for (const auto& floor : *floors) {
                if (!floor.hasCollider || floor.type != "BOX" ||
                    std::abs(position.x - floor.center.x) > floor.size.x * 0.5f ||
                    std::abs(position.z - floor.center.z) > floor.size.z * 0.5f) {
                    continue;
                }
                const float top = floor.center.y + floor.size.y * 0.5f;
                if (!foundFloor || top > groundY) { groundY = top; }
                foundFloor = true;
            }
        }
        if (foundFloor) { position.y = groundY + radius; }
        return position;
    }

    // 両者へ押し戻しを分配し、壁で動けなかった分は反対側へ受け持たせる。
    inline bool Separate(SphereCollider& player, SphereCollider& enemy,
        const std::vector<LevelColliderData>* walls,
        const std::vector<LevelColliderData>* floors)
    {
        if (!Collision::IsHit(player, enemy)) { return false; }
        Vector3 direction{ player.center.x - enemy.center.x, 0.0f,
            player.center.z - enemy.center.z };
        const float distance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        if (distance > 0.0001f) {
            direction.x /= distance;
            direction.z /= distance;
        } else {
            // 中心が一致した場合も、ゼロ除算せずに重なりを解消する。
            direction = { 1.0f, 0.0f, 0.0f };
        }
        const float separation = player.radius + enemy.radius + 0.01f;
        const float push = (std::max)(0.0f, separation - distance);
        player.center = Move(player.center, { direction.x * push * 0.5f, 0.0f, direction.z * push * 0.5f },
            player.radius, walls, floors);
        const float actualSeparation = (player.center.x - enemy.center.x) * direction.x +
            (player.center.z - enemy.center.z) * direction.z;
        const float remaining = (std::max)(0.0f, separation - actualSeparation);
        if (remaining > 0.0001f) {
            enemy.center = Move(enemy.center,
                { -direction.x * remaining, 0.0f, -direction.z * remaining },
                enemy.radius, walls, floors);
        }
        // 敵の背後が壁だった場合は、残った重なりをプレイヤー側で解消する。
        const float finalSeparation = (player.center.x - enemy.center.x) * direction.x +
            (player.center.z - enemy.center.z) * direction.z;
        const float playerRemainder = (std::max)(0.0f, separation - finalSeparation);
        if (playerRemainder > 0.0001f) {
            player.center = Move(player.center,
                { direction.x * playerRemainder, 0.0f, direction.z * playerRemainder },
                player.radius, walls, floors);
        }
        return true;
    }
}
