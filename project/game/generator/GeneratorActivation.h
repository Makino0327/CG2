#pragma once
#include <algorithm>

// 発電機1台分の未起動・起動中・稼働中を状態クラスで管理する。
class GeneratorActivation {
public:
    enum class State { Idle, Starting, Active };
    static constexpr double kActivationSeconds = 15.0; // 起動完了までの秒数。
    bool TryStart() { return state_->TryStart(*this); }
    bool Update(double seconds) { return state_->Update(*this, seconds); }
    State GetState() const { return state_->GetId(); }
    float GetProgress() const { return static_cast<float>(elapsedSeconds_ / kActivationSeconds); }
private:
    class ActivationState {
    public:
        virtual ~ActivationState() = default;
        virtual State GetId() const = 0;
        virtual bool TryStart(GeneratorActivation&) const { return false; }
        virtual bool Update(GeneratorActivation&, double) const { return false; }
    };
    class IdleState final : public ActivationState {
    public:
        State GetId() const override { return State::Idle; }
        bool TryStart(GeneratorActivation& owner) const override {
            // 未起動の状態だけが開始要求を受け付ける。
            owner.ChangeState<StartingState>();
            return true;
        }
    };
    class StartingState final : public ActivationState {
    public:
        State GetId() const override { return State::Starting; }
        bool Update(GeneratorActivation& owner, double seconds) const override {
            if (seconds <= 0.0) { return false; }
            owner.elapsedSeconds_ = (std::min)(owner.elapsedSeconds_ + seconds, kActivationSeconds);
            if (owner.elapsedSeconds_ >= kActivationSeconds) {
                // 完了の通知は稼働中へ切り替わる瞬間に一度だけ返す。
                owner.ChangeState<ActiveState>();
                return true;
            }
            return false;
        }
    };
    class ActiveState final : public ActivationState {
    public:
        State GetId() const override { return State::Active; }
    };
    template<class T> static const ActivationState& GetStateObject() { static const T state; return state; }
    template<class T> void ChangeState() { state_ = &GetStateObject<T>(); }
    const ActivationState* state_ = &GetStateObject<IdleState>();
    double elapsedSeconds_ = 0.0;
};
