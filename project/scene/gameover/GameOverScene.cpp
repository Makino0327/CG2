#include "GameOverScene.h"

// 操作と遷移は共通化し、ゲームオーバー専用の背景と文字を選ぶ。
GameOverScene::GameOverScene() : ResultScene(Type::GameOver) {}
