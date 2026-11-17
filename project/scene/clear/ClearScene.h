#pragma once
#include "../result/ResultScene.h"

// ボス撃破時に確定した戦績を表示するクリア画面。
class ClearScene : public ResultScene
{
public:
    explicit ClearScene(const ClearResult& result);
};
