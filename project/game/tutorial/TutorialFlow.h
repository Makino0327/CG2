#pragma once

// 課題の実行・クリア表示・再挑戦表示・完了待ちを状態クラスで管理する。
class TutorialFlow {
public:
    static constexpr int kStepCount = 7;
    static constexpr int kClearNoticeFrames = 110; // クリア表示後に次の課題へ進む時間。
    static constexpr int kRetryNoticeFrames = 150; // 再挑戦のお知らせを表示する時間。
    static constexpr int kFinishFrames = 240; // 完了表示後にタイトルへ戻るまでの時間。
    enum class Notice { None, Clear, Defeated, Finished };
    void Reset() { *this = TutorialFlow{}; }
    void Update() { ++stepAge_; state_->Update(*this); }
    // 外部は課題の達成や失敗を通知し、次の進行状態は現在の状態が決める。
    void CompleteStep() { state_->CompleteStep(*this); }
    void Finish() { state_->Finish(*this); }
    void ShowRetry(Notice notice) { state_->ShowRetry(*this, notice); }
    int GetStep() const { return step_; }
    int GetClearedStep() const { return clearedStep_; }
    bool IsAcceptingProgress() const { return state_->IsAcceptingProgress(); }
    bool IsDoorOpen(int doorStep) const { return doorStep > 0 && doorStep <= clearedStep_; }
    bool IsStepCleared(int step) const { return step <= clearedStep_; }
    bool IsFinished() const { return finished_; }
    bool IsReadyToLeave() const { return readyToLeave_; }
    Notice GetNotice() const { return notice_; }
    int GetNoticeFrames() const { return noticeFrames_; }
    int GetNoticeDuration() const { return noticeDuration_; }
    int GetStepAge() const { return stepAge_; }
private:
    class State {
    public:
        virtual ~State() = default;
        virtual void Update(TutorialFlow&) const {}
        virtual bool IsAcceptingProgress() const { return true; }
        virtual void CompleteStep(TutorialFlow& owner) const {
            if (owner.clearedStep_ >= owner.step_) { return; }
            owner.clearedStep_ = owner.step_;
            owner.ChangeState<ClearState>(Notice::Clear, kClearNoticeFrames);
        }
        virtual void Finish(TutorialFlow& owner) const {
            owner.clearedStep_ = kStepCount;
            owner.finished_ = true;
            owner.ChangeState<FinishingState>(Notice::Finished, kFinishFrames);
        }
        virtual void ShowRetry(TutorialFlow& owner, Notice notice) const {
            owner.ChangeState<RetryState>(notice, kRetryNoticeFrames);
        }
    };
    class ActiveState final : public State {
    public:
        // 課題の実行中は時間による自動遷移を行わず、達成の通知を待つ。
        void Update(TutorialFlow&) const override {}
    };
    class ClearState final : public State {
    public:
        bool IsAcceptingProgress() const override { return false; }
        void CompleteStep(TutorialFlow&) const override {}
        void ShowRetry(TutorialFlow&, Notice) const override {}
        void Update(TutorialFlow& owner) const override {
            if (--owner.noticeFrames_ == 0) {
                // クリアの表示終了をこの状態自身が判断し、次の課題へ進める。
                owner.step_ = owner.clearedStep_ + 1;
                owner.stepAge_ = 0;
                owner.ChangeState<ActiveState>(Notice::None, 0);
            }
        }
    };
    class RetryState final : public State {
    public:
        void Update(TutorialFlow& owner) const override {
            if (--owner.noticeFrames_ == 0) { owner.ChangeState<ActiveState>(Notice::None, 0); }
        }
    };
    // 完了後は新しい課題や再挑戦で上書きしない。
    class CompletedState : public State {
    public:
        // 完了後は新しい表示タイマーを進めず、呼び出し側の遷移を待つ。
        void Update(TutorialFlow&) const override {}
        bool IsAcceptingProgress() const override { return false; }
        void CompleteStep(TutorialFlow&) const override {}
        void Finish(TutorialFlow&) const override {}
        void ShowRetry(TutorialFlow&, Notice) const override {}
    };
    class FinishingState final : public CompletedState {
    public:
        void Update(TutorialFlow& owner) const override {
            if (--owner.noticeFrames_ == 0) {
                owner.readyToLeave_ = true;
                owner.ChangeState<CompletedState>(Notice::None, 0);
            }
        }
    };
    template<class T> static const State& GetState() { static const T state; return state; }
    template<class T> void ChangeState(Notice notice, int frames) {
        // 表示用の番号は残し、振る舞いの選択には状態オブジェクトを使う。
        state_ = &GetState<T>();
        notice_ = notice;
        noticeFrames_ = frames;
        if (frames > 0) { noticeDuration_ = frames; }
    }
    const State* state_ = &GetState<ActiveState>();
    int step_ = 1;
    int clearedStep_ = 0;
    int stepAge_ = 0;
    Notice notice_ = Notice::None;
    int noticeFrames_ = 0;
    int noticeDuration_ = 1;
    bool finished_ = false;
    bool readyToLeave_ = false;
};
