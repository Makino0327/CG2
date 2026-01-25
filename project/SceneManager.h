#pragma once
#include "BaseScene.h"

class SceneManager
{
public:
	void Update();
	void Draw();
	void SetNextScene(BaseScene* nextScene) {
		nextScene_ = nextScene;
	}

	~SceneManager();

private:
	BaseScene* scene_ = nullptr; // 今実行中
	BaseScene* nextScene_ = nullptr; // 次に切り替える予定


};

