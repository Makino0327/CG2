#pragma once
#include <algorithm>

// ポーズの演出中もゲームを止め、暗転が終わってからタイトルへ切り替える。
class PauseState {
public:
    enum class Phase { Playing, Opening, Open, Closing, Returning, Finished };
    enum class Event { None, Resumed, ReturnToTitle };

    bool IsPaused() const { return phase_ != Phase::Playing; }
    bool CanSelect() const { return phase_ == Phase::Open; }
    Phase GetPhase() const { return phase_; }
    float GetAmount() const { return amount_; }
    float GetReturnAmount() const { return returnAmount_; }
    void Reset() { phase_ = Phase::Playing; amount_ = returnAmount_ = 0.0f; }

    // TABを連続して押しても、現在の明るさから滑らかに開閉する。
    void Toggle() {
        if (phase_ == Phase::Playing || phase_ == Phase::Closing) { phase_ = Phase::Opening; }
        else if (phase_ == Phase::Opening || phase_ == Phase::Open) { Resume(); }
    }
    void Resume() {
        if (phase_ == Phase::Opening || phase_ == Phase::Open) { phase_ = Phase::Closing; }
    }
    void ReturnToTitle() {
        if (phase_ == Phase::Open) { phase_ = Phase::Returning; returnAmount_ = 0.0f; }
    }
    Event Update(float seconds) {
        const float step = (std::max)(seconds, 0.0f)/0.18f;
        if (phase_ == Phase::Opening) {
            amount_ = (std::min)(1.0f, amount_+step);
            if (amount_ >= 1.0f) { phase_ = Phase::Open; }
        } else if (phase_ == Phase::Closing) {
            amount_ = (std::max)(0.0f, amount_-step);
            if (amount_ <= 0.0f) { phase_ = Phase::Playing; return Event::Resumed; }
        } else if (phase_ == Phase::Returning) {
            returnAmount_ = (std::min)(1.0f, returnAmount_+(std::max)(seconds, 0.0f)/0.32f);
            if (returnAmount_ >= 1.0f) { phase_ = Phase::Finished; return Event::ReturnToTitle; }
        }
        return Event::None;
    }
private:
    Phase phase_ = Phase::Playing;
    float amount_ = 0.0f;
    float returnAmount_ = 0.0f;
};
