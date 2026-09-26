#include "GameOverScene.h"

// 見た目と操作はResultSceneで共通化し、ゲームオーバー用の見出しとボタンを選ぶ
GameOverScene::GameOverScene() : ResultScene(Type::GameOver) {}
