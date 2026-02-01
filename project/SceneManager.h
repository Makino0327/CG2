#pragma once
#include "BaseScene.h"
#include <memory>

class SceneManager
{
public:
	void Update();
	void Draw();

	void SetNextScene(std::unique_ptr<BaseScene> nextScene) {
		nextScene_ = std::move(nextScene);
	}

private:
	std::unique_ptr<BaseScene> scene_;      // 今実行中（所有）
	std::unique_ptr<BaseScene> nextScene_;  // 次（所有）


};

