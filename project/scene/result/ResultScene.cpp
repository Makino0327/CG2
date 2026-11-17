#include "ResultScene.h"
#include <algorithm>
#include "../../engine/utils/Logger.h"
#include <cassert>
#include <cmath>
#include <format>
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../SceneManager.h"
#include "../TransitionScanEffect.h"
#include "../title/TitleScene.h"
#include "../gameplay/GamePlayScene.h"
#include "../gameover/GameOverEffect.h"

namespace {
    // タイトルと同じボタン配置・暗転時間
    constexpr int kFadeDuration = 30;
    // デモのゲームオーバーは暗転を含めて3秒（60fpsで180フレーム）で再開する。
    constexpr int kDemoRestartFrames = 3 * 60;
    // クリアのデモでは、戦績を3秒見せてから決定して暗転する。
    constexpr int kDemoClearConfirmFrame = 3 * 60;
    constexpr float kButtonWidth = 240.0f;
    constexpr float kButtonX = (1280.0f - kButtonWidth) * 0.5f;
    constexpr float kButtonHeight = 44.0f;
    constexpr float kButtonTopY = 519.0f;
    // 上から順に戦績を出し、最後の数値のあとにランクを見せる。
    constexpr int kFirstStatFrame = 12;
    constexpr int kStatIntervalFrames = 12;
    constexpr int kStatFadeFrames = 8;
    constexpr int kRankPauseFrames = 16;
    constexpr int kRankFadeFrames = 12;
    // ポーズ画面の選択表示と同じ緑を、見出しとボタンの印に使う。
    constexpr Vector4 kClearAccentColor{ 0.16f, 0.72f, 0.34f, 1 };
    constexpr Vector4 kClearValueColor{ 0.91f, 0.93f, 0.90f, 1 };
    // ランク文字はすべて、モニター表示に合わせた青みのある白にする。
    constexpr Vector4 kClearRankColor{ 0.79f, 0.92f, 1.0f, 1.0f };
    constexpr Vector2 kRankSize{ 144, 144 };

    // result_labels.png の切り出し位置
    constexpr Vector2 kButtonTextureSize{ 512.0f, 64.0f };
    constexpr Vector2 kTitleButtonLeftTop{ 0.0f, 256.0f };
    constexpr Vector2 kRestartButtonLeftTop{ 512.0f, 256.0f };
    constexpr const char* kLabelTexture = "Resources/result/result_labels.png";
    constexpr const char* kClearStatsTexture = "Resources/result/clear_stats.png";

    // ランクはクリアタイムで決める。数字はそれぞれS・A・Bの上限秒数。
    constexpr std::array<double, 3> kRankTimeLimits{ 360.0, 540.0, 720.0 };

    std::string FormatClearTime(double time) {
        const auto seconds = static_cast<long long>(time);
        return std::format("{:02}:{:02}", seconds / 60, seconds % 60);
    }

    // 選択範囲・文字の大きさ・行間をタイトルのメニューと揃える。
    constexpr float kGameOverButtonX = 500.0f;
    constexpr float kGameOverButtonY = 470.0f;
    constexpr float kGameOverButtonWidth = 280.0f;
    constexpr float kGameOverButtonHeight = 44.0f;
    constexpr float kGameOverButtonSpacing = 54.0f;
    constexpr const char* kGameOverLabels = "Resources/gameover/gameover_labels.png";

    // 急に表示を切り替えず、停止時の速度も滑らかなフェードにする。
    float SmoothReveal(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }
}

ResultScene::ResultScene(Type type) : type_(type) {}
ResultScene::ResultScene(const ClearResult& result) : type_(Type::Clear), clearResult_(result) {}
ResultScene::~ResultScene() = default;

Sprite* ResultScene::CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color) {
    // 白いテクスチャへ色を付け、背景と枠線を作る
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/white1x1.png");
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetColor(color);
    sprite->Update();
    Sprite* result = sprite.get();
    uiSprites_.push_back(std::move(sprite));
    return result;
}

Sprite* ResultScene::CreateLabel(const Vector2& textureLeftTop, const Vector2& textureSize,
    const Vector2& position, const Vector2& size, const char* texture) {
    // 文字画像の指定範囲を切り出す。日本語フォントの実行時読み込みは不要
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), texture);
    sprite->SetTextureLeftTop(textureLeftTop);
    sprite->SetTextureSize(textureSize);
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->Update();
    Sprite* result = sprite.get();
    uiSprites_.push_back(std::move(sprite));
    return result;
}

void ResultScene::Initialize() {
    if (initialized_) { return; }
    initialized_ = true;
    assert(GetContext().input && GetContext().dxCommon);
    frame_ = transitionTimer_ = selectedButton_ = 0;
    isLeaving_ = false;
    demoRestartReady_ = false; // 周回前の再開通知を持ち越さない。
    usesDeathBackground_ = type_ == Type::GameOver && GetContext().offscreenRenderer &&
        GetContext().offscreenRenderer->HasDeathSceneCapture();
    if (usesDeathBackground_) { Logger::Log("[DeathSequence] Game over background ready."); }

    if (GetContext().isDebugMode) { *GetContext().isDebugMode = false; }
    if (GetContext().offscreenRenderer) {
        // 本編の死亡時の白黒や構え演出を持ち越さない
        GetContext().offscreenRenderer->ResetDamageFeedback();
        GetContext().offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        for (int effect = 0; effect < static_cast<int>(PostEffectType::Count); ++effect) {
            GetContext().offscreenRenderer->SetPostEffectEnabled(static_cast<PostEffectType>(effect), false);
        }
        GetContext().offscreenRenderer->StopShockwave();
    }

    directionalLightResource_ = GetContext().dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = { 1, 1, 1, 1 };
    light->direction = { 0, -1, 0 };
    light->intensity = 1.0f;

    CreateMenu();
    UpdateMenu();
}

void ResultScene::CreateMenu() {
    if (type_ == Type::GameOver) {
        CreateGameOverMenu();
        return;
    }
    CreateClearSummary();

    // ポーズ画面と同じように、細い緑の印と薄い選択色を使う。
    buttonActions_ = { Action::Title };
    clearButtonBackground_ = CreateUiRect({ kButtonX, kButtonTopY },
        { kButtonWidth, kButtonHeight }, { 0.035f, 0.26f, 0.10f, 0.15f });
    clearButtonMarker_ = CreateUiRect({ kButtonX + 14, kButtonTopY + 12 }, { 3, 20 }, kClearAccentColor);
    CreateLabel(kTitleButtonLeftTop, kButtonTextureSize,
        { kButtonX, kButtonTopY + 7 }, { kButtonWidth, 30 }, kLabelTexture);
    // フェードは一番最後に描画して、画面全体を覆う
    fadeSprite_ = CreateUiRect({ 0,0 }, { 1280,720 }, { 0,0,0,0 });
}

void ResultScene::CreateClearSummary() {
    const Vector4 muted{ 0.56f, 0.62f, 0.57f, 1 };
    const Vector4 divider{ 0.043f, 0.077f, 0.053f, 1 };
    const auto placeText = [this](const Vector2& source, const Vector2& sourceSize,
        const Vector2& position, const Vector2& size, const Vector4& color) {
        Sprite* label = CreateLabel(source, sourceSize, position, size, kClearStatsTexture);
        label->SetColor(color);
        label->Update();
        return label;
    };

    // 操作説明と同じ黒い背景に、緑の走査線と中央のカードを重ねる。
    clearBackground_ = CreateUiRect({ 0, 0 }, { 1280, 720 }, { 0, 0, 0, 1 });
    clearScan_ = std::make_unique<TransitionScanEffect>();
    clearScan_->Initialize(GetContext().spriteCommon, directionalLightResource_.Get());
    // 一枚のパネルに細い仕切りを引き、戦績を一覧で読めるようにする。
    CreateUiRect({ 222, 130 }, { 836, 460 }, divider);
    CreateUiRect({ 223, 131 }, { 834, 458 }, { 0.008f, 0.014f, 0.01f, 1 });
    placeText({ 0, 0 }, { 480, 96 }, { 250, 149 }, { 280, 56 }, kClearAccentColor);
    CreateUiRect({ 250, 210 }, { 780, 1 }, divider);
    CreateUiRect({ 472, 232 }, { 1, 243 }, divider);
    placeText({ 480, 0 }, { 224, 64 }, { 297, 259 }, { 120, 34 }, muted);

    // ランクのS・A・B・Cは同じ大きさで収録している。
    const auto rank = std::find_if(kRankTimeLimits.begin(), kRankTimeLimits.end(), [this](double limit) {
        return clearResult_.clearSeconds < limit;
    });
    const float rankX = 224.0f * static_cast<float>(rank - kRankTimeLimits.begin());
    clearRankValue_ = placeText({ rankX, 400 }, { 224, 224 }, { 357, 359 }, kRankSize, kClearRankColor);
    clearRankValue_->SetAnchorPoint({ 0.5f, 0.5f });

    // 数字を右揃えにして、タイムと回数を読み比べやすくする。
    constexpr char glyphs[] = "0123456789:%-";
    constexpr float digitWidth = 20.0f;
    constexpr float digitSpacing = 13.0f;
    constexpr float valueRight = 1006.0f;
    const std::string accuracy = clearResult_.shotsFired == 0 ? "--" :
        std::to_string(std::lround(100.0 * static_cast<double>(clearResult_.shotsHit) /
            static_cast<double>(clearResult_.shotsFired))) + "%";
    const std::array<std::string, kClearStatCount> values{ FormatClearTime(clearResult_.clearSeconds),
        FormatClearTime(clearResult_.bossSeconds), std::to_string(clearResult_.enemiesDefeated),
        accuracy, std::to_string(clearResult_.damageTakenCount) };
    for (size_t row = 0; row < values.size(); ++row) {
        const float y = 238.0f + 50.0f * static_cast<float>(row);
        if (row > 0) { CreateUiRect({ 504, y - 8 }, { 502, 1 }, divider); }
        placeText({ 0, 104.0f + 48.0f * static_cast<float>(row) }, { 304, 48 },
            { 504, y + 3 }, { 190, 30 }, muted);
        auto& rowSprites = clearStatValues_[row];
        if (row == values.size() - 1 && clearResult_.damageTakenCount == 0) {
            rowSprites.push_back(placeText({ 0, 344 }, { 304, 48 },
                { valueRight - 190, y + 3 }, { 190, 30 }, kClearValueColor));
            continue;
        }
        // 文字画像の余白を少し重ね、数字の間隔を詰める。
        float x = valueRight - digitWidth - digitSpacing * static_cast<float>(values[row].size() - 1);
        for (char digit : values[row]) {
            const auto glyph = std::find(std::begin(glyphs), std::end(glyphs), digit);
            rowSprites.push_back(placeText({ 48.0f * static_cast<float>(glyph - std::begin(glyphs)), 624 },
                { 48, 64 }, { x, y }, { digitWidth, 36 }, kClearValueColor));
            x += digitSpacing;
        }
    }
}

void ResultScene::UpdateClearSummary() {
    // 同じ行の数字と記号を一緒にフェードインさせる。
    for (size_t row = 0; row < clearStatValues_.size(); ++row) {
        const int start = kFirstStatFrame + static_cast<int>(row) * kStatIntervalFrames;
        Vector4 color = kClearValueColor;
        color.w = SmoothReveal(static_cast<float>(frame_ - start) / kStatFadeFrames);
        for (auto* value : clearStatValues_[row]) { value->SetColor(color); }
    }

    // 全行の表示が終わってから、中央を軸にランク文字を出す。
    constexpr int rankFrame = kFirstStatFrame + (static_cast<int>(kClearStatCount) - 1) * kStatIntervalFrames
        + kStatFadeFrames + kRankPauseFrames;
    Vector4 rankColor = kClearRankColor;
    rankColor.w = SmoothReveal(static_cast<float>(frame_ - rankFrame) / kRankFadeFrames);
    clearRankValue_->SetColor(rankColor);
    const float scale = 0.88f + 0.12f * rankColor.w;
    clearRankValue_->SetSize({ kRankSize.x * scale, kRankSize.y * scale });
    clearRankValue_->Update();
}

void ResultScene::CreateGameOverMenu() {
    // 黒い下地の上に、毎フレーム動く筋と煙のパーティクルを描く。
    gameOverBackground_ = CreateUiRect({ 0,0 }, { 1280,720 }, { 0,0,0,1 });
    gameOverEffect_ = std::make_unique<GameOverEffect>();
    gameOverEffect_->Initialize(GetContext().dxCommon);

    const auto label = [this](const Vector2& texturePosition, const Vector2& textureSize,
        const Vector2& position, const Vector2& size) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), kGameOverLabels);
        sprite->SetTextureLeftTop(texturePosition);
        sprite->SetTextureSize(textureSize);
        sprite->SetPosition(position);
        sprite->SetSize(size);
        Sprite* result = sprite.get();
        uiSprites_.push_back(std::move(sprite));
        return result;
    };

    // 見出しは背景と分け、明るい文字を中央へゆっくり浮かび上がらせる。
    gameOverHeadline_ = label({ 0,0 }, { 1280,160 }, { 128,250 }, { 1024,128 });
    buttonActions_ = { Action::Restart, Action::Title };
    buttonLabels_.clear();
    gameOverUnderlines_.clear();
    gameOverIndicators_.clear();
    gameOverButtonGlows_.clear();
    gameOverSelection_.assign(buttonActions_.size(),0.0f);
    for (size_t i = 0; i < buttonActions_.size(); ++i) {
        const float y = kGameOverButtonY + kGameOverButtonSpacing * static_cast<float>(i);
        // 日本語の「リスタート」「タイトルへ戻る」を、タイトルと同じ文字サイズで出す。
        const Vector2 texturePosition = buttonActions_[i] == Action::Restart
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

void ResultScene::UpdateGameOverVisuals() {
    const float age = static_cast<float>(frame_);
    const float headingAlpha = SmoothReveal((age - 16.0f) / 48.0f);
    const float menuAlpha = SmoothReveal((age - 22.0f) / 48.0f);
    // 死亡場所がうっすら残る程度に背景を暗くし、メニュー文字を読みやすくする。
    gameOverBackground_->SetColor({0,0,0,usesDeathBackground_ ? 0.56f*SmoothReveal(age/90.0f) : 1.0f});
    gameOverHeadline_->SetColor({ 1.0f,0.97f,0.94f,headingAlpha });
    for (size_t i = 0; i < buttonLabels_.size(); ++i) {
        // タイトルと同じ速さで文字を明るくし、細い下線を中央から伸ばす。
        const float target = static_cast<int>(i) == selectedButton_ ? 1.0f : 0.0f;
        gameOverSelection_[i] += (target-gameOverSelection_[i])*0.16f;
        const float selected = gameOverSelection_[i];
        // 決定した瞬間が最も明るくなり、約0.2秒で赤い発光を弱める。
        const float flash = isLeaving_ && target > 0.0f
            ? std::exp(-static_cast<float>(transitionTimer_)/11.0f) : 0.0f;
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
    const float fadeAlpha = isLeaving_
        ? SmoothReveal(static_cast<float>(transitionTimer_) / kFadeDuration)
        : (usesDeathBackground_ ? 0.0f : 1.0f - SmoothReveal(age / 40.0f));
    fadeSprite_->SetColor({ 0,0,0,fadeAlpha });
}

void ResultScene::UpdateMenu() {
    const int buttonCount = static_cast<int>(buttonActions_.size());
    const bool gameOver = type_ == Type::GameOver;
    const float buttonX = gameOver ? kGameOverButtonX : kButtonX;
    const float buttonY = gameOver ? kGameOverButtonY : kButtonTopY;
    const float buttonWidth = gameOver ? kGameOverButtonWidth : kButtonWidth;
    const float buttonHeight = gameOver ? kGameOverButtonHeight : kButtonHeight;
    const float buttonSpacing = gameOver ? kGameOverButtonSpacing : 0.0f;
    if (!gameOver) { UpdateClearSummary(); }
    // マウスを動かしたときだけ選択を変え、キーボード操作と競合させない
    const Vector2 mouse = GetContext().input->GetMousePosition();
    const Vector2 delta = GetContext().input->GetMouseDelta();
    int hovered = -1;
    for (int i = 0; i < buttonCount; ++i) {
        const float y = buttonY + buttonSpacing * static_cast<float>(i);
        if (mouse.x >= buttonX && mouse.x <= buttonX+buttonWidth &&
            mouse.y >= y && mouse.y <= y+buttonHeight) { hovered = i; }
    }
    // クリアはランク発表、ゲームオーバーはメニューの表示完了を待つ。
    const bool menuReady = gameOver ? frame_ >= 74 : clearRankValue_->GetColor().w >= 1.0f;
    if (!demoMode_ && !isLeaving_ && menuReady) {
        if (hovered >= 0 && (delta.x != 0.0f || delta.y != 0.0f)) { selectedButton_ = hovered; }
        if (GetContext().input->TriggerKey(DIK_UP) || GetContext().input->TriggerKey(DIK_W)) {
            selectedButton_ = (selectedButton_ + buttonCount - 1) % buttonCount;
        }
        if (GetContext().input->TriggerKey(DIK_DOWN) || GetContext().input->TriggerKey(DIK_S)) {
            selectedButton_ = (selectedButton_ + 1) % buttonCount;
        }
        const bool clicked = hovered >= 0 && GetContext().input->TriggerMouseLeft();
        if (clicked) { selectedButton_ = hovered; }
        if (clicked || GetContext().input->TriggerKey(DIK_RETURN) || GetContext().input->TriggerKey(DIK_SPACE)) {
            isLeaving_ = true;
            transitionTimer_ = 0;
        }
    }

    if (gameOver) {
        UpdateGameOverVisuals();
        for (auto& sprite : uiSprites_) { sprite->Update(); }
    } else {
        // 操作説明と同じ速度と濃さで、背景の走査線を動かす。
        clearScan_->Update(static_cast<float>(frame_), 1.0f);
        // ボタンと画面全体の暗転を更新する。
        const bool highlight = demoMode_ ? isLeaving_ : menuReady && hovered == 0;
        clearButtonBackground_->SetColor({ 0.035f, 0.26f, 0.10f, highlight ? 0.33f : 0.15f });
        Vector4 markerColor = kClearAccentColor;
        markerColor.w = highlight ? 1.0f : 0.6f;
        clearButtonMarker_->SetColor(markerColor);
        const float fade = isLeaving_ ? SmoothReveal(static_cast<float>(transitionTimer_) / kFadeDuration) : 0.0f;
        fadeSprite_->SetColor({ 0, 0, 0, fade });
        fadeSprite_->Update();
    }
}

void ResultScene::Update() {
    ++frame_;
    // デモでは決定を自動で行い、暗転が終わってから次の周回に戻す。
    const int demoConfirmFrame = type_ == Type::Clear ? kDemoClearConfirmFrame
        : kDemoRestartFrames - kFadeDuration + 1;
    if (demoMode_ && !isLeaving_ && frame_ >= demoConfirmFrame) {
        selectedButton_ = 0;
        isLeaving_ = true;
        transitionTimer_ = 0;
    }
    // フェードやメニューの操作中も、背景の筋と煙を別々に動かし続ける。
    if (gameOverEffect_) { gameOverEffect_->Update(1.0f/60.0f); }
    if (isLeaving_) { ++transitionTimer_; }
    UpdateMenu();
    // 暗転が完了した瞬間に一度だけ予約し、読み込み中のシーンを作り直さない
    if (isLeaving_ && transitionTimer_ == kFadeDuration) {
        if (demoMode_) {
            demoRestartReady_ = true;
            return;
        }
        if (buttonActions_[selectedButton_] == Action::Restart) {
            GetSceneManager()->SetNextScene(std::make_unique<GamePlayScene>());
        } else {
            GetSceneManager()->SetNextScene(std::make_unique<TitleScene>());
        }
    }
}

void ResultScene::Draw() {
    if (usesDeathBackground_) { GetContext().offscreenRenderer->DrawDeathBackground(); }
    GetContext().spriteCommon->CommonDrawSetting();
    if (clearScan_) {
        clearBackground_->Draw();
        clearScan_->Draw();
    } else if (gameOverEffect_) {
        gameOverBackground_->Draw();
        gameOverEffect_->Draw(SmoothReveal(static_cast<float>(frame_)/72.0f));
    }
    for (auto& sprite : uiSprites_) {
        // 先に描いた暗い下地を飛ばし、演出の上へカードと文字を重ねる。
        if (sprite.get() == clearBackground_ || sprite.get() == gameOverBackground_) { continue; }
        sprite->Draw();
    }
}

void ResultScene::Finalize() {
    if (type_ == Type::GameOver && GetContext().offscreenRenderer) {
        GetContext().offscreenRenderer->ResetDeathSceneCapture();
    }
    usesDeathBackground_ = false;
    uiSprites_.clear();
    buttonActions_.clear();
    clearButtonBackground_ = nullptr;
    clearButtonMarker_ = nullptr;
    for (auto& row : clearStatValues_) { row.clear(); }
    clearRankValue_ = nullptr;
    clearBackground_ = nullptr;
    clearScan_.reset();
    buttonLabels_.clear();
    gameOverUnderlines_.clear();
    gameOverIndicators_.clear();
    gameOverSelection_.clear();
    gameOverButtonGlows_.clear(); // 周回時に、解放した文字の光を参照しない。
    gameOverEffect_.reset();
    gameOverBackground_ = nullptr;
    gameOverHeadline_ = nullptr;
    fadeSprite_ = nullptr;
    directionalLightResource_.Reset();
    initialized_ = false;
}
