#include "../../project/scene/gameplay/PauseState.h"
#include "../../project/game/tutorial/TutorialFlow.h"
#include "../../project/game/generator/GeneratorActivation.h"
#include "../../project/game/player/Gun.h"
#include "../../project/game/boss/BossCharge.h"
#include "../../project/scene/result/ResultScene.h"
#include "../../project/engine/input/Input.h"
#include <cassert>
#include <iostream>

// 中断・再開・初期化を重ねても、完了通知が重複せず各個体の時間が独立するか確認する。
static void CheckPauseInterruptions() {
    PauseState first;
    first.Toggle();
    first.Update(0.06f);
    PauseState second = first;
    const float amount = first.GetAmount();
    second.Resume();
    assert(second.Update(1.0f) == PauseState::Event::Resumed);
    assert(first.GetAmount() == amount && first.GetPhase() == PauseState::Phase::Opening);
    first.Update(1.0f);
    first.ReturnToTitle();
    first.Resume();
    first.OpenBriefing();
    assert(first.GetPhase() == PauseState::Phase::Returning);
    assert(first.Update(-1.0f) == PauseState::Event::None && first.GetReturnAmount() == 0.0f);
    assert(first.Update(1.0f) == PauseState::Event::ReturnToTitle);
    assert(first.Update(1.0f) == PauseState::Event::None);
    first.Reset();
    first.Toggle();
    assert(first.GetPhase() == PauseState::Phase::Opening && first.GetAmount() == 0.0f);
}

static void CheckTutorialInterruptions() {
    TutorialFlow first;
    first.ShowRetry(TutorialFlow::Notice::Defeated);
    first.CompleteStep();
    assert(first.GetNotice() == TutorialFlow::Notice::Clear && first.IsDoorOpen(1));
    TutorialFlow second = first;
    for (int frame = 0; frame < TutorialFlow::kClearNoticeFrames; ++frame) { second.Update(); }
    assert(second.GetStep() == 2 && first.GetStep() == 1);
    // 最後のポータルに入る通知は、クリア表示中にも優先して完了待ちへ移す。
    first.Finish();
    first.CompleteStep();
    first.ShowRetry(TutorialFlow::Notice::Defeated);
    first.Finish();
    assert(first.GetNotice() == TutorialFlow::Notice::Finished && first.GetNoticeFrames() == TutorialFlow::kFinishFrames);
    for (int frame = 0; frame < TutorialFlow::kFinishFrames; ++frame) { first.Update(); }
    assert(first.IsReadyToLeave() && first.GetStep() == 1);
    first.Update();
    assert(first.GetNoticeFrames() == 0 && first.GetNotice() == TutorialFlow::Notice::None);
    first.Reset();
    first.CompleteStep();
    assert(first.GetNotice() == TutorialFlow::Notice::Clear && !first.IsFinished());
}

static void CheckGeneratorCopies() {
    GeneratorActivation first;
    assert(first.TryStart());
    assert(!first.Update(5.0));
    GeneratorActivation second = first;
    assert(second.Update(10.0));
    assert(first.GetState() == GeneratorActivation::State::Starting);
    assert(first.GetProgress() > 0.33f && first.GetProgress() < 0.34f);
    assert(!second.Update(10.0) && !second.TryStart());
    first = GeneratorActivation{};
    assert(first.GetState() == GeneratorActivation::State::Idle && first.GetProgress() == 0.0f);
}

static void CheckReloadInterruptions() {
    Handgun first;
    Handgun second;
    GunFireContext context;
    context.emitBullet = [](float) { return true; };
    assert(first.TryFire({true, true}, context));
    first.StartReload();
    first.UpdateReload();
    const int timer = first.GetReloadTimer();
    first.StartReload();
    assert(first.GetReloadTimer() == timer && !first.TryFire({true, true}, context));
    assert(second.TryFire({true, true}, context) && !second.IsReloading());
    const int reserve = first.GetReserveAmmo();
    first.CancelReload();
    assert(!first.IsReloading() && first.GetAmmo() == 6 && first.GetReserveAmmo() == reserve);
    first.UpdateReload();
    assert(first.GetAmmo() == 6 && first.GetReloadTimer() == 0);
    first.StartReload();
    first.RefillMagazine();
    assert(!first.IsReloading() && first.GetAmmo() == first.GetMaxAmmo());
}

static void CheckChargeReset() {
    BossCharge first;
    assert(first.Begin({0, 0, 0}, {0, 0, 10}, 1.0f, nullptr));
    for (int frame = 0; frame < BossCharge::kWindupFrames; ++frame) { first.Update(); }
    BossCharge second = first;
    second.Update();
    assert(second.GetPosition().z > 0.0f && first.GetPosition().z == 0.0f);
    first.Reset();
    first.Update();
    assert(!first.IsRunning() && !first.IsWindingUp() && !first.IsFinished());
    assert(!first.TryHit({{0, 0, 0}, 1.0f}));
    assert(first.Begin({0, 0, 0}, {0, 0, 5}, 1.0f, nullptr));
    assert(first.IsWindingUp() && first.GetWindupProgress() == 0.0f);
}

// GPUを使わないリザルトで、実際の共通状態とフレーム順序を確認する。
class TestResultScene final : public ResultScene {
public:
    bool ready = false;
    int contentUpdates = 0;
    void Configure(Input& input) {
        SceneContext context;
        context.input = &input;
        SetContext(context);
        ConfigureMenu({0, 0, 100, 20, 25}, {Action::Title, Action::Restart}, 3);
    }
    int TransitionFrame() const { return GetTransitionTimer(); }
    bool Leaving() const { return IsLeaving(); }
private:
    void CreateContent() override {}
    void UpdateContent() override { ++contentUpdates; }
    bool IsMenuReady() const override { return ready; }
    void UpdateMenuVisuals(bool, int) override {}
    void DrawBackground() override {}
    void FinalizeContent() override {}
};

static void CheckResultSequence() {
    Input input;
    TestResultScene scene;
    scene.Configure(input);
    scene.Update();
    assert(!scene.Leaving() && scene.contentUpdates == 1);
    scene.ready = true;
    scene.Update();
    assert(!scene.Leaving());
    scene.SetDemoMode(true);
    scene.Update();
    // 自動決定のフレームから暗転が進み、30フレーム目にだけ周回可能になる。
    assert(scene.Leaving() && scene.TransitionFrame() == 1 && scene.contentUpdates == 3);
    for (int frame = 0; frame < 28; ++frame) { scene.Update(); }
    assert(!scene.IsDemoRestartReady());
    scene.Update();
    assert(scene.IsDemoRestartReady() && scene.TransitionFrame() == 30);
    scene.Update();
    assert(scene.TransitionFrame() == 31);

    // 自動展示は、メニューの準備待ち中でも指定された時間で決定できる。
    TestResultScene waiting;
    waiting.Configure(input);
    waiting.SetDemoMode(true);
    for (int frame = 0; frame < 3; ++frame) { waiting.Update(); }
    assert(waiting.Leaving() && waiting.TransitionFrame() == 1 && !waiting.ready);
}

int main() {
    CheckPauseInterruptions();
    CheckTutorialInterruptions();
    CheckGeneratorCopies();
    CheckReloadInterruptions();
    CheckChargeReset();
    CheckResultSequence();
    std::cout << "PASS: state interruption, reset, one-shot events and independent instance/copy data\n";
}
