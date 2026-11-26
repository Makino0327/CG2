#include "ClearScene.h"
#include <algorithm>
#include <cmath>
#include <format>
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../TransitionScanEffect.h"

namespace {
    // クリア画面の配置・色・表示時刻は、変更前の値を維持する。
    constexpr int kFadeDuration = 30;
    constexpr int kDemoClearConfirmFrame = 3 * 60;
    constexpr float kButtonWidth = 240.0f;
    constexpr float kButtonX = (1280.0f - kButtonWidth) * 0.5f;
    constexpr float kButtonHeight = 44.0f;
    constexpr float kButtonTopY = 519.0f;
    constexpr int kFirstStatFrame = 12;
    constexpr int kStatIntervalFrames = 12;
    constexpr int kStatFadeFrames = 8;
    constexpr int kRankPauseFrames = 16;
    constexpr int kRankFadeFrames = 12;
    constexpr Vector4 kClearAccentColor{ 0.16f, 0.72f, 0.34f, 1 };
    constexpr Vector4 kClearValueColor{ 0.91f, 0.93f, 0.90f, 1 };
    constexpr Vector4 kClearRankColor{ 0.79f, 0.92f, 1.0f, 1.0f };
    constexpr Vector2 kRankSize{ 144, 144 };
    constexpr Vector2 kButtonTextureSize{ 512.0f, 64.0f };
    constexpr Vector2 kTitleButtonLeftTop{ 0.0f, 256.0f };
    constexpr const char* kLabelTexture = "Resources/result/result_labels.png";
    constexpr const char* kClearStatsTexture = "Resources/result/clear_stats.png";
    constexpr std::array<double, 3> kRankTimeLimits{ 360.0, 540.0, 720.0 };

    std::string FormatClearTime(double time) {
        const auto seconds = static_cast<long long>(time);
        return std::format("{:02}:{:02}", seconds / 60, seconds % 60);
    }

    float SmoothReveal(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }
}

ClearScene::ClearScene(const ClearResult& result) : clearResult_(result) {}
ClearScene::~ClearScene() = default;

void ClearScene::CreateContent() {
    CreateClearSummary();

    // ポーズ画面と同じように、細い緑の印と薄い選択色を使う。
    ConfigureMenu({kButtonX,kButtonTopY,kButtonWidth,kButtonHeight,0.0f}, {Action::Title}, kDemoClearConfirmFrame);
    clearButtonBackground_ = CreateUiRect({ kButtonX, kButtonTopY },
        { kButtonWidth, kButtonHeight }, { 0.035f, 0.26f, 0.10f, 0.15f });
    clearButtonMarker_ = CreateUiRect({ kButtonX + 14, kButtonTopY + 12 }, { 3, 20 }, kClearAccentColor);
    CreateLabel(kTitleButtonLeftTop, kButtonTextureSize,
        { kButtonX, kButtonTopY + 7 }, { kButtonWidth, 30 }, kLabelTexture);
    // フェードは一番最後に描画して、画面全体を覆う
    fadeSprite_ = CreateUiRect({ 0,0 }, { 1280,720 }, { 0,0,0,0 });
}

void ClearScene::CreateClearSummary() {
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
    clearBackground_ = CreateBackground({0,0,0,1});
    clearScan_ = std::make_unique<TransitionScanEffect>();
    clearScan_->Initialize(GetContext().spriteCommon, GetUiLightResource());
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

void ClearScene::UpdateClearSummary() {
    // 同じ行の数字と記号を一緒にフェードインさせる。
    for (size_t row = 0; row < clearStatValues_.size(); ++row) {
        const int start = kFirstStatFrame + static_cast<int>(row) * kStatIntervalFrames;
        Vector4 color = kClearValueColor;
        color.w = SmoothReveal(static_cast<float>(GetFrame() - start) / kStatFadeFrames);
        for (auto* value : clearStatValues_[row]) { value->SetColor(color); }
    }

    // 全行の表示が終わってから、中央を軸にランク文字を出す。
    constexpr int rankFrame = kFirstStatFrame + (static_cast<int>(kClearStatCount) - 1) * kStatIntervalFrames
        + kStatFadeFrames + kRankPauseFrames;
    Vector4 rankColor = kClearRankColor;
    rankColor.w = SmoothReveal(static_cast<float>(GetFrame() - rankFrame) / kRankFadeFrames);
    clearRankValue_->SetColor(rankColor);
    const float scale = 0.88f + 0.12f * rankColor.w;
    clearRankValue_->SetSize({ kRankSize.x * scale, kRankSize.y * scale });
    clearRankValue_->Update();
}

void ClearScene::UpdateContent() {
    // 入力を受け付ける前に、数値とランクの公開状態を確定する。
    UpdateClearSummary();
}

bool ClearScene::IsMenuReady() const { return clearRankValue_->GetColor().w >= 1.0f; }

void ClearScene::UpdateMenuVisuals(bool menuReady, int hovered) {
    // 操作説明と同じ速度と濃さで、背景の走査線を動かす。
    clearScan_->Update(static_cast<float>(GetFrame()), 1.0f);
    // ボタンと画面全体の暗転を更新する。
    const bool highlight = IsDemoMode() ? IsLeaving() : menuReady && hovered == 0;
    clearButtonBackground_->SetColor({ 0.035f, 0.26f, 0.10f, highlight ? 0.33f : 0.15f });
    Vector4 markerColor = kClearAccentColor;
    markerColor.w = highlight ? 1.0f : 0.6f;
    clearButtonMarker_->SetColor(markerColor);
    const float fade = IsLeaving() ? SmoothReveal(static_cast<float>(GetTransitionTimer()) / kFadeDuration) : 0.0f;
    fadeSprite_->SetColor({ 0, 0, 0, fade });
    fadeSprite_->Update();
}

void ClearScene::DrawBackground() {
    GetContext().spriteCommon->CommonDrawSetting();
    clearBackground_->Draw();
    clearScan_->Draw();
}

void ClearScene::FinalizeContent() {
    // 所有する演出と、共通側が持つスプライトへの参照を解除する。
    for (auto& row : clearStatValues_) { row.clear(); }
    clearScan_.reset();
    clearRankValue_ = clearBackground_ = clearButtonBackground_ = clearButtonMarker_ = fadeSprite_ = nullptr;
}
