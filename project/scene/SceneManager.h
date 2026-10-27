#pragma once
#include "BaseScene.h"
#include <memory>

class SceneManager
{
public:
    // 現在のシーンのポーズ状態を、アプリ共通の更新処理へ伝える。
    bool IsPaused() const { return scene_ && scene_->IsPaused(); }
    // 未完成のシーンを準備している間は、動く演出ではなくLoading画面を表示する。
    bool IsLoading() const {
        return nextScene_ && !nextReady_ && (!scene_ || !scene_->KeepDrawingWhileLoading());
    }
	void Update();
	void Draw();
    // 通常画面を描く前に、現在のシーンの影を更新する。
    void DrawShadow();
	// 通常画面とは別に、発光を指定した対象だけを描画する。
	void DrawBloom();
	// 背景へのエフェクトが終わってから、シーンの前面UIを描画する
	void DrawOverlay();

	void SetNextScene(std::unique_ptr<BaseScene> nextScene) {
        // 準備途中のシーンも安全に解放してから、新しい遷移先へ置き換える。
        if (preparingNext_ && nextScene_) { nextScene_->Finalize(); }
        preparingNext_ = nextReady_ = false;
		nextScene_ = std::move(nextScene);
	}

    // 初期化済みのシーンを受け取り、遷移演出中の再初期化を省く。
    void SetNextInitializedScene(std::unique_ptr<BaseScene> nextScene) {
        SetNextScene(std::move(nextScene));
        preparingNext_ = nextReady_ = nextScene_ != nullptr;
    }

	// Gameクラスから初期化時に1回だけContextを受け取る
	void SetContext(const SceneContext& context) { context_ = context; }

	// ... その他 ...
	void DrawImGui(); // ImGui描画呼び出し用

private:
	std::unique_ptr<BaseScene> scene_;      // 今実行中（所有）
	std::unique_ptr<BaseScene> nextScene_;  // 次（所有）
    bool preparingNext_ = false;
    bool nextReady_ = false;
	SceneContext context_;

};

