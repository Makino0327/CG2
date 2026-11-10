#include "GamePlayScene.h"
#include "../title/TitleScene.h"
#include "../SceneManager.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include <algorithm>

namespace {
    // 黒い背景と白い文字に、ゲームの雰囲気に合わせた緑の選択表示を重ねる。
    constexpr float kMenuX = 405.0f;
    constexpr float kRowY[] = { 260.0f, 334.0f, 408.0f };
    enum class PauseOption { Resume, Controls, Title };
    // カードとボタンを同じ比率で縮め、周囲に遷移演出が見える余白を取る。
    constexpr float kBriefingScale = 0.85f;
    constexpr Vector2 kBriefingSize{ 960.0f * kBriefingScale, 540.0f * kBriefingScale };
    constexpr Vector2 kBriefingButtonTextureSize{ 336.0f, 44.0f };
    // 横線の伸縮とカードの開閉に使うフレーム数。
    constexpr int kBriefingLineFrames = 4;
    constexpr int kBriefingOpenFrames = 13;
    constexpr int kBriefingCloseFrames = 10;
    float EasePause(float value) { return value*value*(3.0f-2.0f*value); }

    bool ContainsPoint(const Sprite& sprite, const Vector2& point)
    {
        const Vector2& position = sprite.GetPosition();
        const Vector2& size = sprite.GetSize();
        return point.x >= position.x && point.x <= position.x + size.x &&
            point.y >= position.y && point.y <= position.y + size.y;
    }
}

void GamePlayScene::InitializePauseUi()
{
    pause_.Reset();
    pauseSelection_ = 0;
    pauseSelectionAmounts_ = { 1.0f };
    auto create = [this](const char* texture) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), texture);
        sprite->SetAnchorPoint({ 0, 0 });
        return sprite;
    };
    pauseDim_ = create("Resources/white2x2.png");
    pauseShade_ = create("Resources/title/title_shade.png");
    pausePanel_ = create("Resources/white2x2.png");
    pauseBlackout_ = create("Resources/white2x2.png");
    // ブリーフィングは共通のカードと、場面別の決定ボタンで構成する。
    briefingDim_ = create("Resources/white2x2.png");
    briefingDim_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
    briefingDim_->SetColor({ 0.01f, 0.025f, 0.019f, 0.45f });
    briefingDim_->Update();
    const Vector2 briefingCenter{ WinApp::kClientWidth * 0.5f, WinApp::kClientHeight * 0.5f };
    const Vector2 briefingPosition{ briefingCenter.x - kBriefingSize.x * 0.5f,
        briefingCenter.y - kBriefingSize.y * 0.5f };
    briefingPanel_ = create("Resources/hud/controls_briefing.png");
    briefingPanel_->SetAnchorPoint({ 0.5f, 0.5f });
    briefingPanel_->SetPosition(briefingCenter);
    briefingPanel_->SetSize(kBriefingSize);
    briefingPanel_->Update();
    briefingButton_ = create("Resources/hud/controls_briefing_buttons.png");
    briefingButton_->SetPosition({ briefingPosition.x + 584.0f * kBriefingScale,
        briefingPosition.y + 486.0f * kBriefingScale });
    briefingButton_->SetTextureSize(kBriefingButtonTextureSize);
    briefingButton_->SetSize({ kBriefingButtonTextureSize.x * kBriefingScale,
        kBriefingButtonTextureSize.y * kBriefingScale });
    briefingLine_ = create("Resources/white2x2.png");
    briefingLine_->SetAnchorPoint({ 0.5f, 0.5f });
    briefingLine_->SetPosition(briefingCenter);
    for (size_t i = 0; i < pauseLabels_.size(); ++i) {
        pauseLabels_[i] = create("Resources/hud/pause_labels.png");
        // 1行ずつ切り出し、文字の配置はSpriteの座標で調整する。
        pauseLabels_[i]->SetTextureLeftTop({ 0, static_cast<float>(i)*128.0f });
        pauseLabels_[i]->SetTextureSize({ 1200, 128 });
        pauseLabels_[i]->SetSize({ 600, 64 });
    }
    for (size_t i = 0; i < pauseRows_.size(); ++i) {
        pauseRows_[i] = create("Resources/white2x2.png");
        pauseMarkers_[i] = create("Resources/white2x2.png");
    }
    UpdatePauseVisuals();
}

bool GamePlayScene::UpdatePauseMenu(float seconds)
{
    Input* input = GetContext().input;
    if (!input) { return pause_.IsPaused(); }
    if (pause_.IsBriefingOpen()) {
        // EscやTabは説明だけを閉じ、同じ入力でゲームを再開しない。
        if (UpdateControlsBriefing(false)) { pause_.CloseBriefing(); }
        return true;
    }
    const bool wasPaused = pause_.IsPaused();
    // 通常プレイ中は、ポーズ用Spriteの更新を行わない。
    if (!wasPaused && !input->TriggerKey(DIK_TAB)) { return false; }
    const bool opened = !wasPaused && input->TriggerKey(DIK_TAB);
    if (input->TriggerKey(DIK_TAB)) {
        if (opened) {
            pauseSelection_ = 0;
            pauseSelectionAmounts_ = { 1.0f };
        }
        pause_.Toggle();
    } else if (wasPaused && input->TriggerKey(DIK_ESCAPE)) {
        pause_.Resume();
    }
    const PauseState::Event event = pause_.Update(seconds);
    if (event == PauseState::Event::Resumed && player_) {
        // メニューの決定クリックを、復帰直後のAR連射へ持ち越さない。
        player_->SuppressFireUntilRelease();
    }
    if (event == PauseState::Event::ReturnToTitle) {
        // 黒画面を描くフレームを確保し、次の更新でタイトルを初期化する。
        if (GetContext().isDebugMode) { *GetContext().isDebugMode = false; }
        GetSceneManager()->SetNextScene(std::make_unique<TitleScene>());
    }
    UpdatePauseVisuals();
    if (!opened && pause_.CanSelect()) {
        const Vector2 mouse = input->GetMousePosition();
        const Vector2 delta = input->GetMouseDelta();
        int hovered = -1;
        for (size_t i = 0; i < pauseRows_.size(); ++i) {
            if (ContainsPoint(*pauseRows_[i], mouse)) { hovered = static_cast<int>(i); }
        }
        // マウスを動かした場合だけホバーを使い、キーボードの選択を上書きしない。
        if (hovered >= 0 && (delta.x != 0.0f || delta.y != 0.0f)) { pauseSelection_ = hovered; }
        // 上下キーとW/Sで3項目を順に選ぶ。端まで来たら反対側へ回る。
        const int rowCount = static_cast<int>(pauseRows_.size());
        if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W)) {
            pauseSelection_ = (pauseSelection_ + rowCount - 1) % rowCount;
        } else if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) {
            pauseSelection_ = (pauseSelection_ + 1) % rowCount;
        }
        const bool clicked = hovered >= 0 && input->TriggerMouseLeft();
        if (clicked) { pauseSelection_ = hovered; }
        if (clicked || input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE)) {
            switch (static_cast<PauseOption>(pauseSelection_)) {
            case PauseOption::Resume:
                pause_.Resume();
                break;
            case PauseOption::Controls:
                pause_.OpenBriefing();
                BeginControlsBriefing();
                break;
            case PauseOption::Title:
                pause_.ReturnToTitle();
                break;
            }
        }
        UpdatePauseVisuals();
    }
    // 閉じ終わったフレームも入力を消費し、ゲームの更新は次のフレームから再開する。
    return wasPaused || pause_.IsPaused();
}

void GamePlayScene::UpdatePauseVisuals()
{
    if (!pauseDim_) { return; }
    const float amount = EasePause(pause_.GetAmount());
    const float blackout = EasePause(pause_.GetReturnAmount());
    const float alpha = amount*(1.0f-blackout);
    const float slide = (1.0f-amount)*12.0f;
    auto place = [](Sprite* sprite, Vector2 position, Vector2 size, Vector4 color) {
        sprite->SetPosition(position);
        sprite->SetSize(size);
        sprite->SetColor(color);
        sprite->Update();
    };
    const Vector2 screen{ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };
    place(pauseDim_.get(), { 0, 0 }, screen, { 0.01f, 0.012f, 0.018f, amount*0.68f });
    place(pauseShade_.get(), { 0, 0 }, screen, { 1, 1, 1, amount*0.65f });
    place(pausePanel_.get(), { 374, 140+slide }, { 532, 374 }, { 0.008f, 0.012f, 0.010f, alpha*0.42f });
    // 見出しと3つの選択肢を表示する。
    for (size_t i = 0; i < pauseLabels_.size(); ++i) {
        Vector4 color{ 0.91f, 0.91f, 0.89f, alpha };
        Vector2 position{ kMenuX, 171.0f };
        if (i > 0) {
            const size_t row = i-1;
            position = { 434.0f, kRowY[row] };
            const float target = static_cast<int>(row) == pauseSelection_ ? 1.0f : 0.0f;
            pauseSelectionAmounts_[row] += (target-pauseSelectionAmounts_[row])*0.22f;
            const float selection = pauseSelectionAmounts_[row];
            const float brightness = 0.52f+selection*0.44f;
            color = { brightness, brightness, brightness*0.98f, alpha };
            position.x += selection*5.0f;
            place(pauseRows_[row].get(), { kMenuX, kRowY[row]+slide }, { 475, 64 },
                { 0.035f, 0.26f, 0.10f, alpha*selection*0.33f });
            place(pauseMarkers_[row].get(), { 415, kRowY[row]+22+slide }, { 3, 20 },
                { 0.16f, 0.72f, 0.34f, alpha*selection });
        }
        position.y += slide;
        place(pauseLabels_[i].get(), position, { 600, 64 }, color);
    }
    place(pauseBlackout_.get(), { 0, 0 }, screen, { 0, 0, 0, blackout });
}

void GamePlayScene::BeginControlsBriefing()
{
    // 作戦前とポーズのどちらから開いても、細い横線から始める。
    briefingTransition_ = BriefingTransition::Opening;
    briefingTransitionFrame_ = 0;
    briefingInputReady_ = false;
}

bool GamePlayScene::UpdateControlsBriefing(bool beforeMission)
{
    // 消灯が終わるまでは、作戦開始やポーズメニューへの復帰を待つ。
    if (briefingTransition_ == BriefingTransition::Closing) {
        return ++briefingTransitionFrame_ >= kBriefingCloseFrames;
    }
    if (briefingTransition_ == BriefingTransition::Opening && ++briefingTransitionFrame_ >= kBriefingOpenFrames) {
        briefingTransition_ = BriefingTransition::Open;
    }
    Input* input = GetContext().input;
    if (!input) { return false; }
    if (!briefingInputReady_) {
        // タイトルやポーズで押したボタンが離れてから、ガイドの決定を受け付ける。
        briefingInputReady_ = !input->PushKey(DIK_RETURN) && !input->PushKey(DIK_SPACE) && !input->PushMouseLeft();
        return false;
    }
    if (briefingTransition_ != BriefingTransition::Open) { return false; }
    const bool clicked = input->TriggerMouseLeft() && ContainsPoint(*briefingButton_, input->GetMousePosition());
    if (clicked || input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
        (!beforeMission && (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_TAB)))) {
        briefingTransition_ = BriefingTransition::Closing;
        briefingTransitionFrame_ = 0;
    }
    return false;
}

void GamePlayScene::DrawControlsBriefing(bool beforeMission)
{
    float cardAmount = 1.0f;
    float lineWidth = 0.0f;
    float lineAlpha = 0.0f;
    const float frame = static_cast<float>(briefingTransitionFrame_);
    if (briefingTransition_ == BriefingTransition::Opening) {
        // 点灯時は横線を伸ばし、そこから上下へカードを広げる。
        cardAmount = EasePause(std::clamp((frame - kBriefingLineFrames) /
            (kBriefingOpenFrames - kBriefingLineFrames), 0.0f, 1.0f));
        lineWidth = EasePause(std::min(1.0f, frame / kBriefingLineFrames));
        lineAlpha = (1.0f - cardAmount) * 0.95f;
    } else if (briefingTransition_ == BriefingTransition::Closing) {
        // 消灯時はカードを中央にたたみ、最後に横線を縮めて消す。
        constexpr int kCollapseFrames = kBriefingCloseFrames - kBriefingLineFrames;
        cardAmount = 1.0f - EasePause(std::min(1.0f, frame / kCollapseFrames));
        lineWidth = 1.0f - EasePause(std::clamp((frame - kCollapseFrames) / kBriefingLineFrames, 0.0f, 1.0f));
        lineAlpha = (1.0f - cardAmount) * lineWidth * 0.95f;
    }
    // 作戦前は、カードの外側に読み込み後の走査線をそのまま見せる。
    if (!beforeMission && cardAmount > 0.0f) {
        briefingDim_->SetColor({ 0.01f, 0.025f, 0.019f, 0.45f * cardAmount });
        briefingDim_->Draw();
    }
    if (cardAmount > 0.0f) {
        briefingPanel_->SetSize({ kBriefingSize.x, kBriefingSize.y * cardAmount });
        briefingPanel_->Update();
        briefingPanel_->Draw();
    }
    if (lineWidth > 0.0f && lineAlpha > 0.0f) {
        briefingLine_->SetSize({ kBriefingSize.x * lineWidth, 2.0f });
        briefingLine_->SetColor({ 0.88f, 0.97f, 1.0f, lineAlpha });
        briefingLine_->Update();
        briefingLine_->Draw();
    }
    if (briefingTransition_ != BriefingTransition::Open) { return; }
    // 上段は作戦開始、下段はポーズに戻るボタンを収録している。
    briefingButton_->SetTextureLeftTop({ 0.0f, beforeMission ? 0.0f : kBriefingButtonTextureSize.y });
    Input* input = GetContext().input;
    const float brightness = input && ContainsPoint(*briefingButton_, input->GetMousePosition()) ? 1.0f : 0.88f;
    briefingButton_->SetColor({ brightness, brightness, brightness, 1.0f });
    briefingButton_->Update();
    briefingButton_->Draw();
}

void GamePlayScene::DrawOverlay()
{
    // 倒れている最中は、開始の文字やポーズ画面を重ねない。
    if (player_ && player_->IsDead()) { return; }
    const bool showBriefing = briefingBeforeMission_ || pause_.IsBriefingOpen();
    const bool showMissionStart = missionStartActive_ && missionStartLabel_ && missionStartLine_;
    if (!showBriefing && !showMissionStart && (!pause_.IsPaused() || !pauseDim_)) { return; }
    // 衝撃波やBloomの後に描画し、ポーズの文字を歪ませたり発光させたりしない。
    auto* list = GetContext().dxCommon->GetCommandList();
    const auto rtv = GetContext().dxCommon->GetCurrentBackBufferRTVHandle();
    const auto dsv = GetContext().dxCommon->GetDSVHandle();
    list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    GetContext().spriteCommon->CommonDrawSetting();
    if (showBriefing) {
        DrawControlsBriefing(briefingBeforeMission_);
        return;
    }
    // 作戦開始の文字はBloomや衝撃波の後に重ね、輪郭をくっきり保つ。
    if (showMissionStart) {
        missionStartLine_->Draw();
        missionStartLabel_->Draw();
    }
    if (!pause_.IsPaused() || !pauseDim_) { return; }
    pauseDim_->Draw();
    pauseShade_->Draw();
    pausePanel_->Draw();
    for (auto& row : pauseRows_) { row->Draw(); }
    for (auto& marker : pauseMarkers_) { marker->Draw(); }
    for (auto& label : pauseLabels_) { label->Draw(); }
    pauseBlackout_->Draw();
}
