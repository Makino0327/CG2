#include "Framework.h"
#include "BaseScene.h" // ★ここで完全型にする（Framework.h には入れない）

void Framework::Run()
{
    Initialize();

    // ★Frameworkがシーンを回す
    if (scene_) {
        scene_->Initialize();
    }

    while (true) {

        // Update()
        if (scene_) {
            scene_->Update();
        } else {
            Update();
        }

        if (IsEndRequest()) {
            break;
        }

        // Draw()
        if (scene_) {
            scene_->Draw();
        } else {
            Draw();
        }
    }

    // ★Frameworkがシーンを終わらせる
    if (scene_) {
        scene_->Finalize();
    }

    Finalize();
}
