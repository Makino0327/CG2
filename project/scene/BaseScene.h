// BaseScene.h
#pragma once
#include "SceneContext.h"

class SceneManager;

class BaseScene
{
public:
    virtual ~BaseScene() = default;

    virtual void Initialize() = 0;
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    // ★ ImGui用の仮想関数を追加（必要ないシーンは空でOK）
    virtual void DrawImGui() {}

    virtual void SetSceneManager(SceneManager* sceneManager) {
        sceneManager_ = sceneManager;
    }

    // ★ 共通のコンテキストを受け取る関数を基底クラスに用意
    virtual void SetContext(const SceneContext& context) {
        context_ = context;
    }

protected: // ★ privateではなくprotectedにして、派生先(TitleScene等)から使えるようにする
    SceneManager* sceneManager_ = nullptr;
    SceneContext context_;
};