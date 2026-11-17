#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <memory>
#include <array>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "../BaseScene.h"
#include "../../engine/math/Math.h"

class Sprite;
class GameOverEffect;
class TransitionScanEffect;

// ボスを撃破した時点で確定し、クリア画面へ渡す戦績。
struct ClearResult {
    double clearSeconds = 0.0;
    double bossSeconds = 0.0;
    int enemiesDefeated = 0;
    int damageTakenCount = 0;
    size_t shotsFired = 0;
    size_t shotsHit = 0;
};

// リザルト画面の操作と遷移を共通化し、ゲームオーバーには赤くうねる背景を使う。
class ResultScene : public BaseScene {
public:
    enum class Type { Clear, GameOver };

    explicit ResultScene(Type type);
    explicit ResultScene(const ClearResult& result);
    // 描画クラスの解放は、型の定義を読み込んだcpp側で行う
    ~ResultScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    // 自動プレイでは入力を受けず、表示と暗転の完了を本編へ通知する。
    void SetDemoMode(bool enabled) { demoMode_ = enabled; }
    bool IsDemoRestartReady() const { return demoRestartReady_; }

private:
    // ボタンを決定したときの移動先
    enum class Action { Title, Restart };

    Sprite* CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color);
    Sprite* CreateLabel(const Vector2& textureLeftTop, const Vector2& textureSize,
        const Vector2& position, const Vector2& size, const char* texture);
    void CreateMenu();
    // 撃破時の戦績を、ランクと数値のパネルに並べる。
    void CreateClearSummary();
    void UpdateClearSummary();
    void UpdateMenu();
    // ゲームオーバー専用の背景・見出し・文字だけのメニューを作る。
    void CreateGameOverMenu();
    void UpdateGameOverVisuals();

    Type type_;
    ClearResult clearResult_{};
    bool initialized_ = false;
    bool demoMode_ = false;
    bool demoRestartReady_ = false;
    bool isLeaving_ = false;
    bool usesDeathBackground_ = false;
    int frame_ = 0;
    int transitionTimer_ = 0;
    int selectedButton_ = 0;

    // 枠・文字・ボタン・フェードは通常のSpriteで描く
    std::vector<std::unique_ptr<Sprite>> uiSprites_;
    std::vector<Action> buttonActions_;
    std::vector<Sprite*> buttonLabels_;
    Sprite* clearButtonBackground_ = nullptr;
    Sprite* clearButtonMarker_ = nullptr;
    // 数字とランク文字だけを順番に表示するため、戦績の行ごとにまとめる。
    static constexpr size_t kClearStatCount = 5;
    std::array<std::vector<Sprite*>, kClearStatCount> clearStatValues_;
    Sprite* clearRankValue_ = nullptr;
    // 操作説明と同じ走査線を、黒い背景と戦績カードの間に描く。
    Sprite* clearBackground_ = nullptr;
    std::unique_ptr<TransitionScanEffect> clearScan_;
    Sprite* fadeSprite_ = nullptr;
    // 黒い下地に動く帯と煙を重ね、見出しとメニュー文字は静止させる。
    std::unique_ptr<GameOverEffect> gameOverEffect_;
    Sprite* gameOverBackground_ = nullptr;
    Sprite* gameOverHeadline_ = nullptr;
    std::vector<Sprite*> gameOverUnderlines_;
    // タイトルと同じ小さな赤い印と、滑らかな選択アニメーションを使う。
    std::vector<Sprite*> gameOverIndicators_;
    std::vector<float> gameOverSelection_;
    // 決定した文字の周囲へ薄い赤を重ね、輪郭からにじむ光を作る。
    std::vector<std::array<Sprite*, 16>> gameOverButtonGlows_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
};
