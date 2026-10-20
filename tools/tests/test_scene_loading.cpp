#include "../../project/scene/SceneManager.h"
#include <cassert>
#include <iostream>
#include <memory>

struct State {
    int begins = 0;
    int polls = 0;
    int updates = 0;
    int draws = 0;
    int finalizes = 0;
    int activations = 0;
    int requiredPolls = 1;
    bool canLeave = true;
    float transitionFrame = 0.0f;
    float receivedFrame = 0.0f;
};

class TestScene : public BaseScene {
public:
    explicit TestScene(State& state) : state_(state) {}
    void Initialize() override { assert(GetSceneManager()); ++state_.begins; }
    bool ContinueInitialize() override { return ++state_.polls >= state_.requiredPolls; }
    bool CanLeaveScene() const override { return state_.canLeave; }
    float GetTransitionFrame() const override { return state_.transitionFrame; }
    void OnActivated(float frame) override { ++state_.activations; state_.receivedFrame = frame; }
    void Finalize() override { ++state_.finalizes; }
    void Update() override { ++state_.updates; }
    void Draw() override { ++state_.draws; }
private:
    State& state_;
};

int main() {
    SceneManager manager;
    State title;
    manager.SetNextScene(std::make_unique<TestScene>(title));
    assert(manager.IsLoading());
    manager.Update();
    assert(title.begins == 1 && title.activations == 1);
    assert(!manager.IsLoading());

    // 読み込み中もタイトルを更新・描画し、未完成の本編は表示しない。
    State gameplay;
    gameplay.requiredPolls = 3;
    title.canLeave = false;
    manager.SetNextScene(std::make_unique<TestScene>(gameplay));
    assert(manager.IsLoading());
    for (int frame = 0; frame < 2; ++frame) { manager.Update(); manager.Draw(); }
    assert(title.updates == 3 && title.draws == 2 && title.finalizes == 0);
    assert(gameplay.begins == 1 && gameplay.updates == 0 && gameplay.draws == 0);

    // 準備が完了しても、破片の演出が終わるまでは切り替えない。
    manager.Update();
    manager.Update();
    assert(gameplay.polls == 3 && gameplay.activations == 0);
    assert(!manager.IsLoading());
    title.transitionFrame = 72.0f;
    title.canLeave = true;
    manager.Update();
    manager.Draw();
    assert(title.finalizes == 1 && gameplay.activations == 1);
    assert(gameplay.receivedFrame == 72.0f && gameplay.updates == 1 && gameplay.draws == 1);

    // 準備途中の遷移先を置き換えるときは、古い準備を一度だけ終了する。
    State canceled;
    canceled.requiredPolls = 5;
    manager.SetNextScene(std::make_unique<TestScene>(canceled));
    manager.Update();
    State replacement;
    manager.SetNextScene(std::make_unique<TestScene>(replacement));
    assert(canceled.finalizes == 1 && canceled.activations == 0);
    manager.Update();
    assert(gameplay.finalizes == 1 && replacement.activations == 1);
    assert(canceled.polls == 1);

    // 先読み済みの本編は、Space入力後に初期化や読み込みを繰り返さない。
    State prepared;
    prepared.requiredPolls = 100;
    auto preparedScene = std::make_unique<TestScene>(prepared);
    preparedScene->SetSceneManager(&manager);
    preparedScene->Initialize();
    replacement.canLeave = false;
    manager.SetNextInitializedScene(std::move(preparedScene));
    assert(!manager.IsLoading());
    for (int frame = 0; frame < 120; ++frame) { manager.Update(); manager.Draw(); }
    assert(prepared.begins == 1 && prepared.polls == 0 && prepared.activations == 0);
    assert(prepared.updates == 0 && replacement.finalizes == 0);
    replacement.canLeave = true;
    replacement.transitionFrame = 120.0f;
    manager.Update();
    assert(prepared.begins == 1 && prepared.polls == 0 && prepared.activations == 1);
    assert(prepared.receivedFrame == 120.0f && replacement.finalizes == 1);

    // 先読み済みの遷移を取り消した場合も、保持した本編を一度だけ片付ける。
    State discarded;
    auto discardedScene = std::make_unique<TestScene>(discarded);
    discardedScene->SetSceneManager(&manager);
    discardedScene->Initialize();
    manager.SetNextInitializedScene(std::move(discardedScene));
    State last;
    manager.SetNextScene(std::make_unique<TestScene>(last));
    assert(discarded.finalizes == 1 && discarded.activations == 0);
    assert(manager.IsLoading());
    manager.Update();
    assert(!manager.IsLoading() && last.activations == 1);
    std::cout << "Scene loading tests passed\n";
}
