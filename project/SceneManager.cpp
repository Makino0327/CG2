#include "SceneManager.h"
#include "BaseScene.h"

void SceneManager::Update()
{
    // 次シーンの予約があるなら
    if (nextScene_) {

        // 旧シーンの終了
        if (scene_) {
            scene_->Finalize();
            delete scene_;
            scene_ = nullptr; // ★これ入れる
        }

        // シーン切り替え
        scene_ = nextScene_;
        nextScene_ = nullptr;

        // 次シーン初期化
        scene_->Initialize();
    }

    // ★ここが一番大事：nullなら何もしない
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

SceneManager::~SceneManager()
{
    if (scene_) {
        scene_->Finalize();
        delete scene_;
        scene_ = nullptr;
    }
}

