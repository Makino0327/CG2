#pragma once
#include <algorithm>

// 発電機1台分の起動状態。描画や入力から独立させ、時間だけで進行させる
class GeneratorActivation {
public:
    enum class State { Idle, Starting, Active };

    // 起動完了までの秒数はここで調整する
    static constexpr double kActivationSeconds = 15.0;

    // 未起動のときだけ開始し、連打で進捗がリセットされないようにする
    bool TryStart() {
        if (state_ != State::Idle) {
            return false;
        }
        state_ = State::Starting;
        return true;
    }

    // 経過秒数だけメーターを進め、起動完了した瞬間だけtrueを返す
    bool Update(double deltaSeconds) {
        if (state_ != State::Starting || deltaSeconds <= 0.0) {
            return false;
        }
        elapsedSeconds_ = (std::min)(elapsedSeconds_ + deltaSeconds, kActivationSeconds);
        if (elapsedSeconds_ >= kActivationSeconds) {
            state_ = State::Active;
            return true;
        }
        return false;
    }

    // UIへ渡す状態と、0から1に収まるメーターの割合
    State GetState() const { return state_; }
    float GetProgress() const { return static_cast<float>(elapsedSeconds_ / kActivationSeconds); }

private:
    State state_ = State::Idle;
    double elapsedSeconds_ = 0.0;
};
