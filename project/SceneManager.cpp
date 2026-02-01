#include "SceneManager.h"
#include "BaseScene.h"

void SceneManager::Update()
{
    // 次シーンの予約があるなら
    if (nextScene_) {

        // 旧シーン終了（Finalizeは呼ぶ）
        if (scene_) {
            scene_->Finalize();
        }

        // シーン切り替え（所有権移動）
        scene_ = std::move(nextScene_);

        // Scene に SceneManager を貸す（所有はしない）
        scene_->SetSceneManager(this);

        // 次シーン初期化
        scene_->Initialize();
    }

    if (scene_) {
        scene_->Update();
    }
}

void SceneManager::Draw()
{
    if (scene_) {
        scene_->Draw();
    }
}
