#include "../../project/game/generator/GeneratorActivation.h"
#include "../../project/game/generator/GeneratorDoorLink.h"
#include "../../project/game/generator/GeneratorApproach.h"
#include <cassert>
#include <iostream>

int main() {
    GeneratorDoorLink door;
    // 発電機がない、または未起動の台が残っている間はドアを開かない
    door.Update(0, 0, 30.0);
    door.Update(2, 0, 30.0);
    door.Update(2, 1, 30.0);
    assert(door.GetOpenProgress() == 0.0f);
    assert(!door.IsOpen());

    // 2台目の起動処理が完了するまでは解錠しない
    GeneratorActivation generators[2];
    generators[0].TryStart();
    generators[0].Update(15.0);
    generators[1].TryStart();
    generators[1].Update(14.9);
    size_t count = 0;
    for (const auto& generator : generators) {
        count += generator.GetState() == GeneratorActivation::State::Active ? 1 : 0;
    }
    door.Update(2, count, 1.0);
    assert(door.GetOpenProgress() == 0.0f);
    assert(generators[1].Update(0.2));
    door.Update(2, 2, GeneratorDoorLink::kOpenSeconds * 0.5);
    assert(door.GetOpenProgress() == 0.5f);
    assert(!door.IsOpen());
    door.Update(2, 2, GeneratorDoorLink::kOpenSeconds);
    assert(door.GetOpenProgress() == 1.0f);
    assert(door.IsOpen());
    door.Update(2, 2, 100.0);
    assert(door.GetOpenProgress() == 1.0f);

    // 再読み込みで生成し直せば閉じた状態へ戻る
    door = GeneratorDoorLink{};
    assert(!door.IsOpen());
    assert(door.GetOpenProgress() == 0.0f);

    // 標準サイズと拡縮した発電機の全停止候補が、本体から必要な距離を保つ
    const Vector3 center = { 20.0f, 1.4f, -20.0f };
    const float margin = 2.25f;
    for (const Vector3 size : { Vector3{ 3.2f, 2.8f, 2.0f }, Vector3{ 12.0f, 4.0f, 0.5f } }) {
        const auto points = GetGeneratorApproachPoints(center, size, margin, 1.0f);
        for (const Vector3& point : points) {
            const float dx = point.x - std::clamp(point.x, center.x - size.x * 0.5f, center.x + size.x * 0.5f);
            const float dz = point.z - std::clamp(point.z, center.z - size.z * 0.5f, center.z + size.z * 0.5f);
            assert(std::hypot(dx, dz) >= margin - 0.0001f);
            assert(std::isfinite(point.x) && std::isfinite(point.z));
            assert(point.y == 1.0f);
        }
    }
    std::cout << "PASS: all-generator gate, opening duration, collision release state, reset, approach clearance\n";
}
