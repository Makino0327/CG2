#pragma once
#include "../result/ResultScene.h"
#include <array>
class GameOverEffect;

// 死亡背景と再挑戦メニューの演出を担当するリザルト画面。
class GameOverScene final : public ResultScene {
public:
    GameOverScene();
    ~GameOverScene() override;
private:
    // リザルトの共通操作から、ゲームオーバー専用の処理へ切り替える。
    void CreateContent() override;
    void UpdateContent() override;
    bool IsMenuReady() const override;
    void UpdateMenuVisuals(bool menuReady, int hovered) override;
    void DrawBackground() override;
    void FinalizeContent() override;
    void UpdateGameOverVisuals();
    bool usesDeathBackground_ = false;
    std::unique_ptr<GameOverEffect> gameOverEffect_;
    Sprite* gameOverBackground_ = nullptr;
    Sprite* gameOverHeadline_ = nullptr;
    Sprite* fadeSprite_ = nullptr;
    std::vector<Sprite*> buttonLabels_;
    std::vector<Sprite*> gameOverUnderlines_;
    std::vector<Sprite*> gameOverIndicators_;
    std::vector<float> gameOverSelection_;
    std::vector<std::array<Sprite*,16>> gameOverButtonGlows_;
};
