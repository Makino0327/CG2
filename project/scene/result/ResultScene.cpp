#include "ResultScene.h"
#include <algorithm>
#include "../../engine/utils/Logger.h"
#include <cassert>
#include <cmath>
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
#include "../gameover/GameOverEffect.h"

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
    usesDeathBackground_ = type_ == Type::GameOver && GetContext().offscreenRenderer &&
        GetContext().offscreenRenderer->HasDeathSceneCapture();
    if (usesDeathBackground_) { Logger::Log("[DeathSequence] Game over background ready."); }

    // タイトルと同じ真上に近い固定カメラ
    if (!usesDeathBackground_) {
        GetContext().camera->SetTranslate({ 0.0f, 68.0f, -6.2f });
        GetContext().camera->SetRotate({ 1.48f, 0.0f, 0.0f });
        GetContext().camera->Update();
    }
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

    if (type_ == Type::Clear) {
        // クリア画面は従来の床を使い、ゲームオーバーは専用エフェクトにする。
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
    }

    CreateMenu();
    UpdateMenu();
}

void ResultScene::CreateMenu() {
    if (type_ == Type::GameOver) {
        CreateGameOverMenu();
        return;
    }
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
    buttonBackgrounds_.clear();
    buttonLabels_.clear();
    gameOverUnderlines_.clear();
    gameOverIndicators_.clear();
    gameOverSelection_.assign(buttonActions_.size(),0.0f);
    for (size_t i = 0; i < buttonActions_.size(); ++i) {
        const float y = kGameOverButtonY + kGameOverButtonSpacing * static_cast<float>(i);
        // 日本語の「リスタート」「タイトルへ戻る」を、タイトルと同じ文字サイズで出す。
        const Vector2 texturePosition = buttonActions_[i] == Action::Restart
            ? kRestartButtonLeftTop : kTitleButtonLeftTop;
        buttonLabels_.push_back(CreateLabel(texturePosition,kButtonTextureSize,{520,y+2},{240,36}));
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
        buttonLabels_[i]->SetColor({1,1,1,menuAlpha*(0.42f+0.58f*selected)});
        gameOverIndicators_[i]->SetColor({0.61f,0.14f,0.14f,menuAlpha*selected});
        const float width = 26.0f+50.0f*selected;
        const float y = kGameOverButtonY+kGameOverButtonSpacing*static_cast<float>(i);
        gameOverUnderlines_[i]->SetPosition({640.0f-width*0.5f,y+40.0f});
        gameOverUnderlines_[i]->SetSize({width,1});
        gameOverUnderlines_[i]->SetColor({0.70f,0.63f,0.61f,menuAlpha*selected*0.65f});
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
    const float buttonSpacing = gameOver ? kGameOverButtonSpacing : kButtonSpacing;
    // マウスを動かしたときだけ選択を変え、キーボード操作と競合させない
    const Vector2 mouse = GetContext().input->GetMousePosition();
    const Vector2 delta = GetContext().input->GetMouseDelta();
    int hovered = -1;
    for (int i = 0; i < buttonCount; ++i) {
        const float y = buttonY + buttonSpacing * static_cast<float>(i);
        if (mouse.x >= buttonX && mouse.x <= buttonX+buttonWidth &&
            mouse.y >= y && mouse.y <= y+buttonHeight) { hovered = i; }
    }
    // 見えないメニューを誤って決定しないよう、ゲームオーバーは表示完了まで待つ。
    if (!isLeaving_ && frame_ >= (gameOver ? 74 : kInputDelayFrames)) {
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
    } else {
        // クリア画面は従来の白黒ボタンと暗転を使う。
        for (int i = 0; i < buttonCount; ++i) {
            const bool selected = i == selectedButton_;
            buttonBackgrounds_[i]->SetColor(selected ? Vector4{ 1,1,1,1 } : Vector4{ 0,0,0,0.96f });
            buttonLabels_[i]->SetColor(selected ? Vector4{ 0,0,0,1 } : Vector4{ 1,1,1,1 });
        }
        fadeSprite_->SetColor({ 0,0,0,isLeaving_ ? std::clamp(static_cast<float>(transitionTimer_)/static_cast<float>(kFadeDuration),0.0f,1.0f) : 0.0f });
    }
    for (auto& sprite : uiSprites_) { sprite->Update(); }
}

void ResultScene::Update() {
    ++frame_;
    // フェードやメニューの操作中も、背景の筋と煙を別々に動かし続ける。
    if (gameOverEffect_) { gameOverEffect_->Update(1.0f/60.0f); }
    if (isLeaving_) { ++transitionTimer_; }
    UpdateMenu();
    // 暗転が完了した瞬間に一度だけ予約し、読み込み中のシーンを作り直さない
    if (isLeaving_ && transitionTimer_ == kFadeDuration) {
        if (buttonActions_[selectedButton_] == Action::Restart) {
            GetSceneManager()->SetNextScene(std::make_unique<GamePlayScene>());
        } else {
            GetSceneManager()->SetNextScene(std::make_unique<TitleScene>());
        }
    }
}

void ResultScene::Draw() {
    if (floor_) {
        GetContext().object3dCommon->CommonDrawSetting();
        floor_->Draw();
    }
    GetContext().spriteCommon->CommonDrawSetting();
    if (gameOverEffect_) {
        // 保存した死亡場所を先にぼかして描き、その上へ鮮明な文字と赤い筋を重ねる。
        if (usesDeathBackground_) { GetContext().offscreenRenderer->DrawDeathBackground(); }
        gameOverBackground_->Draw();
        gameOverEffect_->Draw(SmoothReveal(static_cast<float>(frame_)/72.0f));
    }
    for (auto& sprite : uiSprites_) {
        // 黒い下地を二重に描いて、背景エフェクトを隠さないようにする。
        if (gameOverEffect_ && sprite.get() == gameOverBackground_) { continue; }
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
    buttonBackgrounds_.clear();
    buttonLabels_.clear();
    gameOverUnderlines_.clear();
    gameOverIndicators_.clear();
    gameOverSelection_.clear();
    gameOverEffect_.reset();
    gameOverBackground_ = nullptr;
    gameOverHeadline_ = nullptr;
    fadeSprite_ = nullptr;
    floor_.reset();
    directionalLightResource_.Reset();
    initialized_ = false;
}
