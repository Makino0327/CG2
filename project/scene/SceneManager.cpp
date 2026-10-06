#include "SceneManager.h"
#include "BaseScene.h"

// SceneManager.cpp の変更部分
void SceneManager::Update()
{
    if (nextScene_) {
        if (scene_) {
            scene_->Finalize();
        }

        scene_ = std::move(nextScene_);
        scene_->SetSceneManager(this);

        // ★ ここで SceneManager が保持している Context を自動で渡す！
        scene_->SetContext(context_);

        scene_->Initialize();
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

void SceneManager::DrawOverlay()
{
    // 通常の描画と同じシーンへ、前面UIの描画を渡す
    if (scene_) {
        scene_->DrawOverlay();
    }
}
