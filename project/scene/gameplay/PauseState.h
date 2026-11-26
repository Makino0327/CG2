#pragma once
#include <algorithm>

// ポーズの各状態が入力への反応と次の状態を決め、ゲームの停止時間も管理する。
class PauseState {
public:
    enum class Phase { Playing, Opening, Open, Briefing, Closing, Returning, Finished };
    enum class Event { None, Resumed, ReturnToTitle };
    bool IsPaused() const { return GetPhase() != Phase::Playing; }
    bool CanSelect() const { return GetPhase() == Phase::Open; }
    bool IsBriefingOpen() const { return GetPhase() == Phase::Briefing; }
    Phase GetPhase() const { return state_->GetPhase(); }
    float GetAmount() const { return amount_; }
    float GetReturnAmount() const { return returnAmount_; }
    void Reset() { *this = PauseState{}; }
    // 外部からは通知だけを渡し、許可する操作は各状態が選ぶ。
    void Toggle() { state_->Toggle(*this); }
    void Resume() { state_->Resume(*this); }
    void OpenBriefing() { state_->OpenBriefing(*this); }
    void CloseBriefing() { state_->CloseBriefing(*this); }
    void ReturnToTitle() { state_->ReturnToTitle(*this); }
    Event Update(float seconds) { return state_->Update(*this, (std::max)(seconds, 0.0f)); }
private:
    // 指定しない通知は無視し、別の状態でだけ有効な操作を受け付けない。
    class State {
    public:
        virtual ~State() = default;
        virtual Phase GetPhase() const = 0;
        virtual Event Update(PauseState&, float) const { return Event::None; }
        virtual void Toggle(PauseState&) const {}
        virtual void Resume(PauseState&) const {}
        virtual void OpenBriefing(PauseState&) const {}
        virtual void CloseBriefing(PauseState&) const {}
        virtual void ReturnToTitle(PauseState&) const {}
    };
    class PlayingState final : public State {
    public:
        Phase GetPhase() const override { return Phase::Playing; }
        void Toggle(PauseState& owner) const override { owner.ChangeState<OpeningState>(); }
    };
    class OpeningState final : public State {
    public:
        Phase GetPhase() const override { return Phase::Opening; }
        void Toggle(PauseState& owner) const override { Resume(owner); }
        void Resume(PauseState& owner) const override { owner.ChangeState<ClosingState>(); }
        Event Update(PauseState& owner, float seconds) const override {
            // 開き切った時点で選択可能な状態へ進む。
            owner.amount_ = (std::min)(1.0f, owner.amount_ + seconds / 0.18f);
            if (owner.amount_ >= 1.0f) { owner.ChangeState<OpenState>(); }
            return Event::None;
        }
    };
    class OpenState final : public State {
    public:
        Phase GetPhase() const override { return Phase::Open; }
        void Toggle(PauseState& owner) const override { Resume(owner); }
        void Resume(PauseState& owner) const override { owner.ChangeState<ClosingState>(); }
        void OpenBriefing(PauseState& owner) const override { owner.ChangeState<BriefingState>(); }
        void ReturnToTitle(PauseState& owner) const override {
            owner.returnAmount_ = 0.0f;
            owner.ChangeState<ReturningState>();
        }
    };
    class BriefingState final : public State {
    public:
        Phase GetPhase() const override { return Phase::Briefing; }
        // ガイドを閉じても、プレイは再開せずポーズ画面へ戻す。
        void CloseBriefing(PauseState& owner) const override { owner.ChangeState<OpenState>(); }
    };
    class ClosingState final : public State {
    public:
        Phase GetPhase() const override { return Phase::Closing; }
        void Toggle(PauseState& owner) const override { owner.ChangeState<OpeningState>(); }
        Event Update(PauseState& owner, float seconds) const override {
            owner.amount_ = (std::max)(0.0f, owner.amount_ - seconds / 0.18f);
            if (owner.amount_ <= 0.0f) {
                owner.ChangeState<PlayingState>();
                return Event::Resumed;
            }
            return Event::None;
        }
    };
    class ReturningState final : public State {
    public:
        Phase GetPhase() const override { return Phase::Returning; }
        Event Update(PauseState& owner, float seconds) const override {
            owner.returnAmount_ = (std::min)(1.0f, owner.returnAmount_ + seconds / 0.32f);
            if (owner.returnAmount_ >= 1.0f) {
                owner.ChangeState<FinishedState>();
                return Event::ReturnToTitle;
            }
            return Event::None;
        }
    };
    class FinishedState final : public State {
    public:
        Phase GetPhase() const override { return Phase::Finished; }
    };
    // 不変の状態オブジェクトを共有し、遷移時のメモリ確保を省く。
    template<class T> static const State& GetState() { static const T state; return state; }
    template<class T> void ChangeState() { state_ = &GetState<T>(); }
    const State* state_ = &GetState<PlayingState>();
    float amount_ = 0.0f;
    float returnAmount_ = 0.0f;
};
