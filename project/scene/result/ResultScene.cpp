#include "ResultScene.h"
#include <algorithm>
#include <cassert>
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../../engine/3d/model/ModelManager.h"
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../game/camera/Camera.h"
#include "../SceneManager.h"
#include "../title/TitleScene.h"
#include "../gameplay/GamePlayScene.h"

namespace {
    // タイトルと同じボタン配置・暗転時間
    constexpr int kFadeDuration = 30;
    constexpr float kButtonX = 480.0f;
    constexpr float kButtonWidth = 320.0f;
    constexpr float kButtonHeight = 54.0f;
    constexpr float kButtonTopY = 548.0f;
    constexpr float kButtonSpacing = 66.0f;
    // 前のシーンで押していたキーで、いきなり決定しないための待ち時間
    constexpr int kInputDelayFrames = 20;

    // result_labels.png の切り出し位置
    constexpr Vector2 kHeadlineTextureSize{ 1024.0f, 128.0f };
    constexpr Vector2 kClearHeadlineLeftTop{ 0.0f, 0.0f };
    constexpr Vector2 kGameOverHeadlineLeftTop{ 0.0f, 128.0f };
    constexpr Vector2 kButtonTextureSize{ 512.0f, 64.0f };
    constexpr Vector2 kTitleButtonLeftTop{ 0.0f, 256.0f };
    constexpr Vector2 kRestartButtonLeftTop{ 512.0f, 256.0f };
    constexpr const char* kLabelTexture = "Resources/result/result_labels.png";
}

ResultScene::ResultScene(Type type) : type_(type) {}
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
    const Vector2& position, const Vector2& size) {
    // 文字画像の指定範囲を切り出す。日本語フォントの実行時読み込みは不要
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), kLabelTexture);
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
    assert(GetContext().camera && GetContext().input && GetContext().dxCommon);
    frame_ = transitionTimer_ = selectedButton_ = 0;
    isLeaving_ = false;

    // タイトルと同じ真上に近い固定カメラ
    GetContext().camera->SetTranslate({ 0.0f, 68.0f, -6.2f });
    GetContext().camera->SetRotate({ 1.48f, 0.0f, 0.0f });
    GetContext().camera->Update();
    if (GetContext().isDebugMode) { *GetContext().isDebugMode = false; }
    if (GetContext().offscreenRenderer) {
        // 本編の死亡時の白黒や構え演出を持ち越さない
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

    // タイトルと同じ床だけを背景に置く
    ModelManager::GetInstance()->LoadModel("cube.obj");
    floor_ = std::make_unique<Object3d>();
    floor_->Initialize(GetContext().object3dCommon);
    floor_->SetCamera(GetContext().camera);
    floor_->SetModel("cube.obj");
    floor_->SetTranslate({ 0.0f, -1.0f, 0.0f });
    floor_->SetScale({ 50.0f, 1.0f, 50.0f });
    floor_->SetColor({ 1, 1, 1, 1 });
    floor_->SetLightingType(LightingType::HalfLambert);
    floor_->Update();

    CreateMenu();
    UpdateMenu();
}

void ResultScene::CreateMenu() {
    // 上部の見出しは枠を付けず、文字だけを大きく出す(クリアは赤、ゲームオーバーは青)
    Sprite* headline = CreateLabel(type_ == Type::Clear ? kClearHeadlineLeftTop : kGameOverHeadlineLeftTop,
        kHeadlineTextureSize, { 160,50 }, { 960,120 });
    headline->SetColor(type_ == Type::Clear ? Vector4{ 1.0f,0.12f,0.12f,1 } : Vector4{ 0.15f,0.45f,1.0f,1 });

    // クリアはタイトルへ戻るだけ、ゲームオーバーはリスタートとタイトルへ戻るを選ぶ
    buttonActions_.clear();
    if (type_ == Type::GameOver) { buttonActions_.push_back(Action::Restart); }
    buttonActions_.push_back(Action::Title);

    buttonBackgrounds_.clear();
    buttonLabels_.clear();
    for (size_t i = 0; i < buttonActions_.size(); ++i) {
        const float y = kButtonTopY + kButtonSpacing * static_cast<float>(i);
        CreateUiRect({ kButtonX,y }, { kButtonWidth,kButtonHeight }, { 1,1,1,1 });
        buttonBackgrounds_.push_back(CreateUiRect({ kButtonX+2,y+2 }, { kButtonWidth-4,kButtonHeight-4 }, { 0,0,0,0.96f }));
        const Vector2 leftTop = buttonActions_[i] == Action::Restart ? kRestartButtonLeftTop : kTitleButtonLeftTop;
        buttonLabels_.push_back(CreateLabel(leftTop, kButtonTextureSize, { kButtonX,y+7 }, { kButtonWidth,40 }));
    }
    // フェードは一番最後に描画して、画面全体を覆う
    fadeSprite_ = CreateUiRect({ 0,0 }, { 1280,720 }, { 0,0,0,0 });
}

void ResultScene::UpdateMenu() {
    const int buttonCount = static_cast<int>(buttonActions_.size());
    // マウスを動かしたときだけ選択を変え、キーボード操作と競合させない
    const Vector2 mouse = GetContext().input->GetMousePosition();
    const Vector2 delta = GetContext().input->GetMouseDelta();
    int hovered = -1;
    for (int i = 0; i < buttonCount; ++i) {
        const float y = kButtonTopY + kButtonSpacing * static_cast<float>(i);
        if (mouse.x >= kButtonX && mouse.x <= kButtonX+kButtonWidth &&
            mouse.y >= y && mouse.y <= y+kButtonHeight) { hovered = i; }
    }
    if (!isLeaving_ && frame_ >= kInputDelayFrames) {
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

    // 選択中は白地に黒文字、非選択は黒地に白文字(タイトルと同じ)
    for (int i = 0; i < buttonCount; ++i) {
        const bool selected = i == selectedButton_;
        buttonBackgrounds_[i]->SetColor(selected ? Vector4{ 1,1,1,1 } : Vector4{ 0,0,0,0.96f });
        buttonLabels_[i]->SetColor(selected ? Vector4{ 0,0,0,1 } : Vector4{ 1,1,1,1 });
    }
    fadeSprite_->SetColor({ 0,0,0,isLeaving_ ? std::clamp(static_cast<float>(transitionTimer_)/static_cast<float>(kFadeDuration),0.0f,1.0f) : 0.0f });
    for (auto& sprite : uiSprites_) { sprite->Update(); }
}

void ResultScene::Update() {
    ++frame_;
    if (isLeaving_) { ++transitionTimer_; }
    UpdateMenu();
    // 暗転し切ったフレームで移動先のシーンを予約する
    if (isLeaving_ && transitionTimer_ >= kFadeDuration) {
        if (buttonActions_[selectedButton_] == Action::Restart) {
            GetSceneManager()->SetNextScene(std::make_unique<GamePlayScene>());
        } else {
            GetSceneManager()->SetNextScene(std::make_unique<TitleScene>());
        }
    }
}

void ResultScene::Draw() {
    GetContext().object3dCommon->CommonDrawSetting();
    floor_->Draw();
    GetContext().spriteCommon->CommonDrawSetting();
    for (auto& sprite : uiSprites_) { sprite->Draw(); }
}

void ResultScene::Finalize() {
    uiSprites_.clear();
    buttonActions_.clear();
    buttonBackgrounds_.clear();
    buttonLabels_.clear();
    fadeSprite_ = nullptr;
    floor_.reset();
    directionalLightResource_.Reset();
    initialized_ = false;
}
