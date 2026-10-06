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
    // 個別指定された光だけをBloom用画像へ描く。必要なシーンだけ実装する。
    virtual void DrawBloom() {}

    // ポストエフェクトの影響を受けないUIを最後に重ねる。必要なシーンだけ実装する
    virtual void DrawOverlay() {}

    // ★ ImGui用の仮想関数を追加（必要ないシーンは空でOK）
    virtual void DrawImGui() {}

    virtual void SetSceneManager(SceneManager* sceneManager) {
        sceneManager_ = sceneManager;
    }

    // ★ 共通のコンテキストを受け取る関数を基底クラスに用意
    virtual void SetContext(const SceneContext& context) {
        context_ = context;
    }

protected:
    // 派生シーンには読み取り用の関数だけを公開し、メンバ変数自体は書き換えさせない
    SceneManager* GetSceneManager() const { return sceneManager_; }
    const SceneContext& GetContext() const { return context_; }

private:
    SceneManager* sceneManager_ = nullptr;
    SceneContext context_;
};
