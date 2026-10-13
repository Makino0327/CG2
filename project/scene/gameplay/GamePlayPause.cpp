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
    constexpr float kRowY[] = { 302.0f, 376.0f };
    float EasePause(float value) { return value*value*(3.0f-2.0f*value); }
}

void GamePlayScene::InitializePauseUi()
{
    pause_.Reset();
    pauseSelection_ = 0;
    pauseSelectionAmounts_ = { 1.0f, 0.0f };
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
    const bool wasPaused = pause_.IsPaused();
    // 通常プレイ中は、ポーズ用Spriteの更新を行わない。
    if (!wasPaused && !input->TriggerKey(DIK_TAB)) { return false; }
    const bool opened = !wasPaused && input->TriggerKey(DIK_TAB);
    if (input->TriggerKey(DIK_TAB)) {
        if (opened) {
            pauseSelection_ = 0;
            pauseSelectionAmounts_ = { 1.0f, 0.0f };
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
            const Vector2 position = pauseRows_[i]->GetPosition();
            if (mouse.x >= position.x && mouse.x <= position.x+475.0f &&
                mouse.y >= position.y && mouse.y <= position.y+64.0f) { hovered = static_cast<int>(i); }
        }
        // マウスを動かした場合だけホバーを使い、キーボードの選択を上書きしない。
        if (hovered >= 0 && (delta.x != 0.0f || delta.y != 0.0f)) { pauseSelection_ = hovered; }
        if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_DOWN) ||
            input->TriggerKey(DIK_W) || input->TriggerKey(DIK_S)) { pauseSelection_ = 1-pauseSelection_; }
        const bool clicked = hovered >= 0 && input->TriggerMouseLeft();
        if (clicked) { pauseSelection_ = hovered; }
        if (clicked || input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE)) {
            if (pauseSelection_ == 0) { pause_.Resume(); }
            else { pause_.ReturnToTitle(); }
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
    place(pausePanel_.get(), { 374, 154+slide }, { 532, 360 }, { 0.008f, 0.012f, 0.010f, alpha*0.42f });
    // 見出しと2つの選択肢だけを表示する。
    const Vector2 labelPositions[] = { { 405, 192 }, { 434, kRowY[0] },
        { 434, kRowY[1] } };
    for (size_t i = 0; i < pauseLabels_.size(); ++i) {
        Vector4 color{ 0.91f, 0.91f, 0.89f, alpha };
        Vector2 position = labelPositions[i];
        if (i == 1 || i == 2) {
            const size_t row = i-1;
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

void GamePlayScene::DrawOverlay()
{
    if (!pause_.IsPaused() || !pauseDim_) { return; }
    // 衝撃波やBloomの後に描画し、ポーズの文字を歪ませたり発光させたりしない。
    auto* list = GetContext().dxCommon->GetCommandList();
    const auto rtv = GetContext().dxCommon->GetCurrentBackBufferRTVHandle();
    const auto dsv = GetContext().dxCommon->GetDSVHandle();
    list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    GetContext().spriteCommon->CommonDrawSetting();
    pauseDim_->Draw();
    pauseShade_->Draw();
    pausePanel_->Draw();
    for (auto& row : pauseRows_) { row->Draw(); }
    for (auto& marker : pauseMarkers_) { marker->Draw(); }
    for (auto& label : pauseLabels_) { label->Draw(); }
    pauseBlackout_->Draw();
}
