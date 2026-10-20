#include "SceneManager.h"
#include "BaseScene.h"

// SceneManager.cpp の変更部分
void SceneManager::Update()
{
    if (nextScene_) {
        if (!preparingNext_) {
            nextScene_->SetSceneManager(this);
            nextScene_->SetContext(context_);
            nextScene_->BeginInitialize();
            preparingNext_ = true;
        }
        if (!nextReady_) { nextReady_ = nextScene_->ContinueInitialize(); }
        // 読み込み中は元のシーンを保持し、遷移演出と描画を動かし続ける。
        if (nextReady_ && (!scene_ || scene_->CanLeaveScene())) {
            const float transitionFrame = scene_ ? scene_->GetTransitionFrame() : 0.0f;
            if (scene_) { scene_->Finalize(); }
            scene_ = std::move(nextScene_);
            preparingNext_ = nextReady_ = false;
            scene_->OnActivated(transitionFrame);
        }
    }

    if (scene_) {
        scene_->Update();
    }
}

// ImGui用も追加しておく
void SceneManager::DrawImGui() {
    if (scene_) {
        scene_->DrawImGui();
    }
}

void SceneManager::Draw()
{
    if (scene_) {
        scene_->Draw();
    }
}

void SceneManager::DrawBloom()
{
    if (scene_) { scene_->DrawBloom(); }
}

void SceneManager::DrawShadow()
{
    // シーンごとに指定された3Dモデルだけを、光の視点から描く。
    if (scene_) { scene_->DrawShadow(); }
}

void SceneManager::DrawOverlay()
{
    // 通常の描画と同じシーンへ、前面UIの描画を渡す
    if (scene_) {
        scene_->DrawOverlay();
    }
}
