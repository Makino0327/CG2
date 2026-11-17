#include "../../project/game/tutorial/TutorialFlow.h"
#include <cassert>
#include <iostream>

int main() {
    TutorialFlow flow;
    // 最初は1番目の課題で、ドアはどれも閉じている
    assert(flow.GetStep() == 1);
    assert(!flow.IsDoorOpen(1));
    assert(flow.IsAcceptingProgress());

    // 達成するとドアはすぐ開くが、課題の表示は「クリア！」の後で切り替わる
    flow.CompleteStep();
    assert(flow.IsDoorOpen(1));
    assert(!flow.IsDoorOpen(2));
    assert(flow.GetStep() == 1);
    assert(!flow.IsAcceptingProgress());
    // 同じ課題を二重に達成しない
    flow.CompleteStep();
    assert(flow.GetClearedStep() == 1);
    for (int frame = 0; frame < TutorialFlow::kClearNoticeFrames; ++frame) { flow.Update(); }
    assert(flow.GetStep() == 2);
    assert(flow.GetNotice() == TutorialFlow::Notice::None);
    assert(flow.IsAcceptingProgress());

    // 失敗のお知らせは課題を進めず、時間が来たら消える
    flow.ShowRetry(TutorialFlow::Notice::Defeated);
    assert(flow.GetNotice() == TutorialFlow::Notice::Defeated);
    for (int frame = 0; frame < TutorialFlow::kRetryNoticeFrames; ++frame) { flow.Update(); }
    assert(flow.GetStep() == 2);
    assert(flow.GetNotice() == TutorialFlow::Notice::None);

    // 6番目まで進めると、最後の課題になる
    for (int step = 2; step <= 6; ++step) {
        flow.CompleteStep();
        // クリア表示中は失敗のお知らせで上書きしない
        flow.ShowRetry(TutorialFlow::Notice::Defeated);
        assert(flow.GetNotice() == TutorialFlow::Notice::Clear);
        for (int frame = 0; frame < TutorialFlow::kClearNoticeFrames; ++frame) { flow.Update(); }
    }
    assert(flow.GetStep() == 7);
    assert(flow.IsDoorOpen(6));
    assert(!flow.IsDoorOpen(7));

    // ポータルに入ったら完了を見せ、その後にタイトルへ戻れる
    flow.Finish();
    assert(flow.IsFinished());
    assert(!flow.IsReadyToLeave());
    for (int frame = 0; frame < TutorialFlow::kFinishFrames; ++frame) { flow.Update(); }
    assert(flow.IsReadyToLeave());

    flow.Reset();
    assert(flow.GetStep() == 1 && !flow.IsFinished());

    std::cout << "test_tutorial_flow passed\n";
    return 0;
}
