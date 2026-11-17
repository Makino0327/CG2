#include "ClearScene.h"

// 撃破時点の戦績を、見た目と操作を担当するResultSceneへ渡す。
ClearScene::ClearScene(const ClearResult& result) : ResultScene(result) {}
