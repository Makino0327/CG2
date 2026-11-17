#pragma once

// チュートリアルの進み具合と、画面に出すお知らせの時間だけを管理する。
// 敵を倒したかなどの判定はシーン側で行い、達成したら CompleteStep を呼ぶ。
class TutorialFlow {
public:
    static constexpr int kStepCount = 7;
    // 「クリア！」を見せてから次の課題へ切り替えるまでのフレーム数
    static constexpr int kClearNoticeFrames = 110;
    // やられた時のお知らせを出しておくフレーム数
    static constexpr int kRetryNoticeFrames = 150;
    // 完了の文字を見せてからタイトルへ戻るまでのフレーム数
    static constexpr int kFinishFrames = 240;

    enum class Notice { None, Clear, Defeated, Finished };

    void Reset() { *this = TutorialFlow{}; }

    void Update() {
        ++stepAge_;
        if (noticeFrames_ > 0 && --noticeFrames_ == 0) {
            // 「クリア！」を出し終えたら、次の部屋の課題を表示する
            if (notice_ == Notice::Clear) {
                step_ = clearedStep_ + 1;
                stepAge_ = 0;
            }
            if (notice_ == Notice::Finished) { readyToLeave_ = true; }
            notice_ = Notice::None;
        }
    }

    // 今の課題を達成した。ドアはすぐ開き、課題の表示はお知らせの後で切り替える。
    void CompleteStep() {
        if (finished_ || notice_ == Notice::Clear || clearedStep_ >= step_) { return; }
        clearedStep_ = step_;
        ShowNotice(Notice::Clear, kClearNoticeFrames);
    }

    // 最後のポータルに入った
    void Finish() {
        if (finished_) { return; }
        clearedStep_ = kStepCount;
        finished_ = true;
        ShowNotice(Notice::Finished, kFinishFrames);
    }

    // 失敗のお知らせ。クリアや完了の表示中は上書きしない。
    void ShowRetry(Notice notice) {
        if (finished_ || notice_ == Notice::Clear) { return; }
        ShowNotice(notice, kRetryNoticeFrames);
    }

    int GetStep() const { return step_; }
    int GetClearedStep() const { return clearedStep_; }
    // 課題の判定を受け付けるか(クリア表示中や完了後は受け付けない)
    bool IsAcceptingProgress() const { return !finished_ && notice_ != Notice::Clear; }
    // TutorialDoor_N は、ステップNを達成した時に開く
    bool IsDoorOpen(int doorStep) const { return doorStep > 0 && doorStep <= clearedStep_; }
    bool IsStepCleared(int step) const { return step <= clearedStep_; }
    bool IsFinished() const { return finished_; }
    bool IsReadyToLeave() const { return readyToLeave_; }
    Notice GetNotice() const { return notice_; }
    int GetNoticeFrames() const { return noticeFrames_; }
    int GetNoticeDuration() const { return noticeDuration_; }
    // 課題が切り替わってからの経過フレーム(表示の出現演出に使う)
    int GetStepAge() const { return stepAge_; }

private:
    void ShowNotice(Notice notice, int frames) {
        notice_ = notice;
        noticeFrames_ = frames;
        noticeDuration_ = frames;
    }

    int step_ = 1;
    int clearedStep_ = 0;
    int stepAge_ = 0;
    Notice notice_ = Notice::None;
    int noticeFrames_ = 0;
    int noticeDuration_ = 1;
    bool finished_ = false;
    bool readyToLeave_ = false;
};
