#pragma once
#include "../result/ResultScene.h"

// テレポーターを踏んだときのクリア画面。床の背景に STAGE CLEAR と「タイトルへ戻る」を出す
class ClearScene : public ResultScene
{
public:
    ClearScene();
};
