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
    // 決定後の暗転時間は、すべてのリザルトで共通にする。
    constexpr int kFadeDuration = 30;
}

ResultScene::~ResultScene() = default;

void ResultScene::ConfigureMenu(const MenuLayout& layout, std::initializer_list<Action> actions, int demoConfirmFrame) {
    // 配置と移動先をまとめて設定し、部分的な変更で不整合を作らない。
    menuLayout_ = layout;
    buttonActions_ = actions;
    demoConfirmFrame_ = demoConfirmFrame;
}

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

Sprite* ResultScene::CreateBackground(const Vector4& color) {
    // 背景は派生側で先に描くため、共通の前景描画から除外する。
    background_ = CreateUiRect({0,0}, {1280,720}, color);
    return background_;
}

void ResultScene::UpdateSprites() {
    // 所有コンテナを公開せず、必要な一括更新だけを提供する。
    for (auto& sprite : uiSprites_) { sprite->Update(); }
}

void ResultScene::Initialize() {
    if (initialized_) { return; }
    initialized_ = true;
    assert(GetContext().input && GetContext().dxCommon);
    frame_ = transitionTimer_ = selectedButton_ = 0;
    isLeaving_ = false;
    demoRestartReady_ = false; // 周回前の再開通知を持ち越さない。

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

    CreateContent();
    UpdateContent();
    UpdateMenu();
}

void ResultScene::UpdateMenu() {
    const int buttonCount = static_cast<int>(buttonActions_.size());
    const auto layout = menuLayout_;
    // マウスを動かしたときだけ選択を変え、キーボード操作と競合させない
    const Vector2 mouse = GetContext().input->GetMousePosition();
    const Vector2 delta = GetContext().input->GetMouseDelta();
    int hovered = -1;
    for (int i = 0; i < buttonCount; ++i) {
        const float y = layout.y + layout.spacing * static_cast<float>(i);
        if (mouse.x >= layout.x && mouse.x <= layout.x+layout.width &&
            mouse.y >= y && mouse.y <= y+layout.height) { hovered = i; }
    }
    // クリアはランク発表、ゲームオーバーはメニューの表示完了を待つ。
    const bool menuReady = IsMenuReady();
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

    // 入力結果を渡し、画面ごとの選択演出を仮想関数で更新する。
    UpdateMenuVisuals(menuReady, hovered);
}

void ResultScene::Update() {
    ++frame_;
    // デモでは決定を自動で行い、暗転が終わってから次の周回に戻す。
    if (demoMode_ && !isLeaving_ && frame_ >= demoConfirmFrame_) {
        selectedButton_ = 0;
        isLeaving_ = true;
        transitionTimer_ = 0;
    }
    // フェードやメニューの操作中も、背景の筋と煙を別々に動かし続ける。
    UpdateContent();
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
    // 基底型から呼んでも、実際の画面に対応した背景へ切り替わる。
    DrawBackground();
    for (auto& sprite : uiSprites_) {
        if (sprite.get() != background_) { sprite->Draw(); }
    }
}

void ResultScene::Finalize() {
    if (!initialized_) { return; }
    // 派生側の参照を先に解除してから、共通の描画リソースを破棄する。
    FinalizeContent();
    background_ = nullptr;
    uiSprites_.clear();
    buttonActions_.clear();
    directionalLightResource_.Reset();
    initialized_ = false;
}
