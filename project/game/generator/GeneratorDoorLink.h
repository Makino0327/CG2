#pragma once
#include <algorithm>
#include <cstddef>

// 全発電機の起動完了を受けて、ステージ内のドアを一度だけ開く
class GeneratorDoorLink {
public:
    static constexpr double kOpenSeconds = 1.25; // ドアが上へ開き切る秒数

    void Update(size_t generatorCount, size_t activeCount, double deltaSeconds) {
        // 発電機が0台のステージでは自動的に開かない
        if (generatorCount > 0 && activeCount == generatorCount) {
            unlocked_ = true;
        }
        if (unlocked_ && deltaSeconds > 0.0) {
            elapsedSeconds_ = (std::min)(elapsedSeconds_ + deltaSeconds, kOpenSeconds);
        }
    }

    // 描画位置と通行判定が同じ開き具合を参照する
    float GetOpenProgress() const { return static_cast<float>(elapsedSeconds_ / kOpenSeconds); }
    bool IsOpen() const { return elapsedSeconds_ >= kOpenSeconds; }

private:
    bool unlocked_ = false;
    double elapsedSeconds_ = 0.0;
};
