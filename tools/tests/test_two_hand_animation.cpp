#include "../../project/engine/animation/Animation.h"
#include <cassert>
#include <cmath>
#include <iostream>

static float QuaternionSimilarity(const Quaternion& a, const Quaternion& b)
{
    return std::abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
}

int main()
{
    // ゲームと同じローダーで実ファイルを読み、両手のキーが欠けていないことを確認する。
    const std::string directory = "project/Resources/player_modular";
    const auto names = GetAnimationNames(directory, "SwatTwoHand.gltf");
    assert(names.size() == 2 && names[0] == "TwoHand_Aim" && names[1] == "TwoHand_Shoot");
    const Animation aim = LoadAnimationFile(directory, "SwatTwoHand.gltf", 0);
    const Animation shot = LoadAnimationFile(directory, "SwatTwoHand.gltf", 1);
    assert(std::abs(shot.duration - 0.32f) < 0.001f);
    for (const char* name : { "Wrist.L", "Wrist.R", "LowerArm.L", "LowerArm.R", "Chest" }) {
        const auto& aimed = aim.nodeAnimations.at(name);
        const auto& fired = shot.nodeAnimations.at(name);
        const auto neutral = CalculateValue(aimed.rotate.keyframes, 0.0f);
        const auto recovered = CalculateValue(fired.rotate.keyframes, shot.duration);
        // 反動の最後が両手構えへ戻り、モーション切り替えで腕が跳ばないことを確認する。
        assert(QuaternionSimilarity(neutral, recovered) > 0.9999f);
        for (int frame = 0; frame <= 20; ++frame) {
            const auto q = CalculateValue(fired.rotate.keyframes, frame * shot.duration / 20.0f);
            assert(std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w));
            assert(std::abs(QuaternionSimilarity(q, q) - 1.0f) < 0.002f);
        }
    }
    // 胸が固定されたままの腕上げではなく、反動中に姿勢が変化することを確認する。
    const auto& chest = shot.nodeAnimations.at("Chest").rotate.keyframes;
    assert(QuaternionSimilarity(CalculateValue(chest, 0.0f), CalculateValue(chest, 0.05f)) < 0.99999f);
    std::cout << "Two-hand animation loader, recovery and interpolation: PASS\n";
}
