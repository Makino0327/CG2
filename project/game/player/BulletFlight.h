#pragma once
#include "../../engine/math/Math.h"
#include <cmath>
#include <optional>

// 弾の移動と高さへの到達を共通化し、ゲームと再現テストで同じ軌道を使う。
inline void AdvanceBulletFlight(Vector3& position, Vector3& velocity,
    const std::optional<float>& flightHeight)
{
    position = { position.x+velocity.x, position.y+velocity.y, position.z+velocity.z };
    if (!flightHeight || !((velocity.y < 0.0f && position.y <= *flightHeight) ||
        (velocity.y > 0.0f && position.y >= *flightHeight))) { return; }
    // 狙う高さへ届いた後は水平に進め、床の下へ軌跡が消えるのを防ぐ。
    position.y = *flightHeight;
    const float speed = std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y+velocity.z*velocity.z);
    const float horizontal = std::sqrt(velocity.x*velocity.x+velocity.z*velocity.z);
    if (horizontal <= 0.0f) { velocity = { 0, 0, 0 }; return; }
    velocity = { velocity.x/horizontal*speed, 0.0f, velocity.z/horizontal*speed };
}
