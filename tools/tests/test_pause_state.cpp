#include "../../project/scene/gameplay/PauseState.h"
#include <cassert>
#include <iostream>

int main()
{
    PauseState pause;
    assert(!pause.IsPaused());
    // TABで開き始めた瞬間からゲームを止め、演出が終わったら選択を受け付ける。
    pause.Toggle();
    assert(pause.IsPaused() && !pause.CanSelect());
    pause.Update(0.09f);
    assert(pause.GetAmount() > 0.49f && pause.GetAmount() < 0.51f);
    pause.Update(0.09f);
    assert(pause.CanSelect() && pause.GetAmount() == 1.0f);
    // 閉じる演出中も敵や発電機を止め、再開の通知を一度だけ返す。
    pause.Resume();
    assert(pause.Update(0.09f) == PauseState::Event::None && pause.IsPaused());
    assert(pause.Update(0.09f) == PauseState::Event::Resumed && !pause.IsPaused());
    assert(pause.Update(1.0f) == PauseState::Event::None);
    // 閉じかけでTABを押しても、明るさを飛ばさず再び開く。
    pause.Toggle();
    pause.Update(0.18f);
    pause.Toggle();
    pause.Update(0.06f);
    const float amount = pause.GetAmount();
    pause.Toggle();
    assert(pause.GetAmount() == amount && pause.IsPaused());
    pause.Update(0.18f);
    // タイトルへ戻る場合は、暗転が完了するまでゲームを止めて待つ。
    pause.ReturnToTitle();
    pause.Toggle();
    assert(pause.GetPhase() == PauseState::Phase::Returning);
    assert(pause.Update(0.16f) == PauseState::Event::None && pause.IsPaused());
    assert(pause.Update(0.16f) == PauseState::Event::ReturnToTitle);
    assert(pause.GetReturnAmount() == 1.0f && pause.IsPaused());
    assert(pause.Update(1.0f) == PauseState::Event::None);
    pause.Reset();
    assert(!pause.IsPaused() && pause.GetReturnAmount() == 0.0f);
    std::cout << "Pause transitions and gameplay blocking checks passed.\n";
}
