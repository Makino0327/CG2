#pragma once
#include "../result/ResultScene.h"
#include <array>
class TransitionScanEffect;

// 戦績とランクの表示だけを担当するリザルト画面。
class ClearScene final : public ResultScene {
public:
    explicit ClearScene(const ClearResult& result);
    ~ClearScene() override;
private:
    // 基底クラスの共通操作から呼ばれる、クリア画面固有の振る舞い。
    void CreateContent() override;
    void UpdateContent() override;
    bool IsMenuReady() const override;
    void UpdateMenuVisuals(bool menuReady, int hovered) override;
    void DrawBackground() override;
    void FinalizeContent() override;
    void CreateClearSummary();
    void UpdateClearSummary();
    ClearResult clearResult_;
    static constexpr size_t kClearStatCount = 5;
    std::array<std::vector<Sprite*>, kClearStatCount> clearStatValues_;
    Sprite* clearRankValue_ = nullptr;
    Sprite* clearBackground_ = nullptr;
    Sprite* clearButtonBackground_ = nullptr;
    Sprite* clearButtonMarker_ = nullptr;
    Sprite* fadeSprite_ = nullptr;
    std::unique_ptr<TransitionScanEffect> clearScan_;
};
