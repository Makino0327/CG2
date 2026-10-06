#pragma once
#include "BaseScene.h"
#include <memory>

class SceneManager
{
public:
	void Update();
	void Draw();
    // 通常画面を描く前に、現在のシーンの影を更新する。
    void DrawShadow();
	// 通常画面とは別に、発光を指定した対象だけを描画する。
	void DrawBloom();
	// 背景へのエフェクトが終わってから、シーンの前面UIを描画する
	void DrawOverlay();

	void SetNextScene(std::unique_ptr<BaseScene> nextScene) {
		nextScene_ = std::move(nextScene);
	}

	// Gameクラスから初期化時に1回だけContextを受け取る
	void SetContext(const SceneContext& context) { context_ = context; }

	// ... その他 ...
	void DrawImGui(); // ImGui描画呼び出し用

private:
	std::unique_ptr<BaseScene> scene_;      // 今実行中（所有）
	std::unique_ptr<BaseScene> nextScene_;  // 次（所有）
	SceneContext context_;

};

