#include "GameOverScene.h"
#include <algorithm>
#include <cmath>
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../engine/utils/Logger.h"
#include "GameOverEffect.h"

namespace {
    // ゲームオーバー画面の配置・色・表示時刻は、変更前の値を維持する。
    constexpr int kFadeDuration = 30;
    constexpr int kDemoRestartFrames = 3 * 60;
    constexpr Vector2 kButtonTextureSize{ 512.0f, 64.0f };
    constexpr Vector2 kTitleButtonLeftTop{ 0.0f, 256.0f };
    constexpr Vector2 kRestartButtonLeftTop{ 512.0f, 256.0f };
    constexpr const char* kLabelTexture = "Resources/result/result_labels.png";
    constexpr float kGameOverButtonX = 500.0f;
    constexpr float kGameOverButtonY = 470.0f;
    constexpr float kGameOverButtonWidth = 280.0f;
    constexpr float kGameOverButtonHeight = 44.0f;
    constexpr float kGameOverButtonSpacing = 54.0f;
    constexpr const char* kGameOverLabels = "Resources/gameover/gameover_labels.png";

    float SmoothReveal(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }
}

GameOverScene::GameOverScene() = default;
GameOverScene::~GameOverScene() = default;

void GameOverScene::CreateContent() {
    // 死亡背景の利用可否は、この画面の内部だけで判断する。
    usesDeathBackground_ = GetContext().offscreenRenderer && GetContext().offscreenRenderer->HasDeathSceneCapture();
    if (usesDeathBackground_) { Logger::Log("[DeathSequence] Game over background ready."); }
    // 黒い下地の上に、毎フレーム動く筋と煙のパーティクルを描く。
    gameOverBackground_ = CreateBackground({0,0,0,1});
    gameOverEffect_ = std::make_unique<GameOverEffect>();
    gameOverEffect_->Initialize(GetContext().dxCommon);


    // 見出しは背景と分け、明るい文字を中央へゆっくり浮かび上がらせる。
    gameOverHeadline_ = CreateLabel({0,0},{1280,160},{128,250},{1024,128},kGameOverLabels);
    ConfigureMenu({kGameOverButtonX,kGameOverButtonY,kGameOverButtonWidth,kGameOverButtonHeight,kGameOverButtonSpacing},
        {Action::Restart,Action::Title}, kDemoRestartFrames-kFadeDuration+1);
    buttonLabels_.clear();
    gameOverUnderlines_.clear();
    gameOverIndicators_.clear();
    gameOverButtonGlows_.clear();
    gameOverSelection_.assign(GetButtonCount(),0.0f);
    for (size_t i = 0; i < GetButtonCount(); ++i) {
        const float y = kGameOverButtonY + kGameOverButtonSpacing * static_cast<float>(i);
        // 日本語の「リスタート」「タイトルへ戻る」を、タイトルと同じ文字サイズで出す。
        const Vector2 texturePosition = GetButtonAction(i) == Action::Restart
            ? kRestartButtonLeftTop : kTitleButtonLeftTop;
        // 文字より先に薄い輪郭を描き、中央の文字を読めるまま赤い光をにじませる。
        std::array<Sprite*, 16> glows{};
        for (auto& glow : glows) {
            glow = CreateLabel(texturePosition,kButtonTextureSize,{520,y+2},{240,36},kLabelTexture);
            glow->SetColor({1.0f,0.015f,0.025f,0.0f});
        }
        gameOverButtonGlows_.push_back(glows);
        buttonLabels_.push_back(CreateLabel(texturePosition,kButtonTextureSize,{520,y+2},{240,36},kLabelTexture));
        gameOverIndicators_.push_back(CreateUiRect({576,y+17},{3,3},{0.61f,0.14f,0.14f,0}));
        gameOverUnderlines_.push_back(CreateUiRect({602,y+40},{76,1},{0.70f,0.63f,0.61f,0}));
    }
    // 入場時と決定後の暗転は、必ず文字より手前に描く。
    fadeSprite_ = CreateUiRect({ 0,0 }, { 1280,720 }, { 0,0,0,1 });
}

void GameOverScene::UpdateGameOverVisuals() {
    const float age = static_cast<float>(GetFrame());
    const float headingAlpha = SmoothReveal((age - 16.0f) / 48.0f);
    const float menuAlpha = SmoothReveal((age - 22.0f) / 48.0f);
    // 死亡場所がうっすら残る程度に背景を暗くし、メニュー文字を読みやすくする。
    gameOverBackground_->SetColor({0,0,0,usesDeathBackground_ ? 0.56f*SmoothReveal(age/90.0f) : 1.0f});
    gameOverHeadline_->SetColor({ 1.0f,0.97f,0.94f,headingAlpha });
    for (size_t i = 0; i < buttonLabels_.size(); ++i) {
        // タイトルと同じ速さで文字を明るくし、細い下線を中央から伸ばす。
        const float target = static_cast<int>(i) == GetSelectedButton() ? 1.0f : 0.0f;
        gameOverSelection_[i] += (target-gameOverSelection_[i])*0.16f;
        const float selected = gameOverSelection_[i];
        // 決定した瞬間が最も明るくなり、約0.2秒で赤い発光を弱める。
        const float flash = IsLeaving() && target > 0.0f
            ? std::exp(-static_cast<float>(GetTransitionTimer())/11.0f) : 0.0f;
        buttonLabels_[i]->SetColor({1.0f,1.0f-0.86f*flash,1.0f-0.82f*flash,
            menuAlpha*(0.42f+0.58f*selected)});
        gameOverIndicators_[i]->SetColor({0.61f+0.39f*flash,0.14f-0.12f*flash,
            0.14f-0.10f*flash,menuAlpha*selected});
        const float width = 26.0f+50.0f*selected+52.0f*flash;
        const float y = kGameOverButtonY+kGameOverButtonSpacing*static_cast<float>(i);
        for (size_t j = 0; j < gameOverButtonGlows_[i].size(); ++j) {
            // 内側を濃く、外側を薄く重ね、発光が文字の外へ少し広がるようにする。
            const float angle = static_cast<float>(j%8)*0.78539816f;
            const bool inner = j < 8;
            const float radius = (inner ? 1.5f : 4.0f)+(1.0f-flash)*1.5f;
            Sprite* glow = gameOverButtonGlows_[i][j];
            glow->SetPosition({520.0f+std::cos(angle)*radius,y+2.0f+std::sin(angle)*radius});
            glow->SetColor({1.0f,0.015f,0.025f,menuAlpha*flash*(inner ? 0.13f : 0.055f)});
        }
        gameOverUnderlines_[i]->SetPosition({640.0f-width*0.5f,y+40.0f});
        gameOverUnderlines_[i]->SetSize({width,1.0f+1.5f*flash});
        gameOverUnderlines_[i]->SetColor({0.70f+0.30f*flash,0.63f-0.60f*flash,
            0.61f-0.57f*flash,menuAlpha*selected*(0.65f+0.35f*flash)});
    }
    const float fadeAlpha = IsLeaving()
        ? SmoothReveal(static_cast<float>(GetTransitionTimer()) / kFadeDuration)
        : (usesDeathBackground_ ? 0.0f : 1.0f - SmoothReveal(age / 40.0f));
    fadeSprite_->SetColor({ 0,0,0,fadeAlpha });
}

void GameOverScene::UpdateContent() {
    // 初期表示では時刻を進めず、通常更新から背景の動作を開始する。
    if (GetFrame() > 0) { gameOverEffect_->Update(1.0f/60.0f); }
}

bool GameOverScene::IsMenuReady() const { return GetFrame() >= 74; }

void GameOverScene::UpdateMenuVisuals(bool, int) {
    UpdateGameOverVisuals();
    UpdateSprites();
}

void GameOverScene::DrawBackground() {
    if (usesDeathBackground_) { GetContext().offscreenRenderer->DrawDeathBackground(); }
    GetContext().spriteCommon->CommonDrawSetting();
    gameOverBackground_->Draw();
    gameOverEffect_->Draw(SmoothReveal(static_cast<float>(GetFrame())/72.0f));
}

void GameOverScene::FinalizeContent() {
    // 死亡キャプチャの寿命と赤い演出を、この画面の終了時に片付ける。
    if (GetContext().offscreenRenderer) { GetContext().offscreenRenderer->ResetDeathSceneCapture(); }
    usesDeathBackground_ = false;
    gameOverEffect_.reset();
    buttonLabels_.clear();
    gameOverUnderlines_.clear();
    gameOverIndicators_.clear();
    gameOverSelection_.clear();
    gameOverButtonGlows_.clear();
    gameOverBackground_ = gameOverHeadline_ = fadeSprite_ = nullptr;
}
