#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "../../engine/math/Math.h"

// 発電機のBOX外周へ余白を足し、敵が停止する候補位置を作る
inline std::array<Vector3, 16> GetGeneratorApproachPoints(
    const Vector3& center, const Vector3& size, float margin, float groundY)
{
    std::array<Vector3, 16> points{};
    const float halfX = std::fabs(size.x) * 0.5f + margin;
    const float halfZ = std::fabs(size.z) * 0.5f + margin;
    for (size_t index = 0; index < points.size(); ++index) {
        const float angle = static_cast<float>(index) * 6.28318530f / static_cast<float>(points.size());
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        // 楕円ではなく余白付きBOXの外周へ置き、角でも本体にめり込ませない
        const float distance = (std::min)(
            halfX / (std::max)(std::fabs(x), 0.0001f),
            halfZ / (std::max)(std::fabs(z), 0.0001f));
        points[index] = { center.x + x * distance, groundY, center.z + z * distance };
    }
    return points;
}
