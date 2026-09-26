#pragma once
#include "../result/ResultScene.h"

// プレイヤー死亡時のゲームオーバー画面。床の背景に GAME OVER と「リスタート」「タイトルへ戻る」を出す
class GameOverScene : public ResultScene
{
public:
    GameOverScene();
};
