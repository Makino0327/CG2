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

// クリアとゲームオーバーの表示内容に関係なく、同じ進行状態を利用する。
class ResultScene::State {
public:
    virtual ~State() = default;
    virtual void Update(ResultScene& owner) const = 0;
    virtual bool CanSelect() const { return false; }
    virtual bool IsLeaving() const { return false; }
    virtual void OnReady(ResultScene&) const {}
    virtual void OnConfirm(ResultScene&) const {}
    bool UpdateAutoConfirm(ResultScene& owner) const {
        if (!owner.demoMode_ || owner.frame_ < owner.demoConfirmFrame_) { return false; }
        owner.selectedButton_ = 0;
        owner.transitionTimer_ = 0;
        owner.ChangeState(GetState<LeavingState>());
        // 自動決定は通常の入力処理より前なので、このフレームから暗転を進める。
        owner.state_->Update(owner);
        return true;
    }
};

class ResultScene::PresentingState final : public ResultScene::State {
public:
    void OnReady(ResultScene& owner) const override {
        if (owner.IsMenuReady()) { owner.ChangeState(GetState<MenuState>()); }
    }
    void Update(ResultScene& owner) const override {
        if (UpdateAutoConfirm(owner)) { return; }
        owner.UpdateContent();
        OnReady(owner);
        owner.UpdateMenu();
    }
};

class ResultScene::MenuState final : public ResultScene::State {
public:
    bool CanSelect() const override { return true; }
    void OnConfirm(ResultScene& owner) const override {
        owner.transitionTimer_ = 0;
        owner.ChangeState(GetState<LeavingState>());
    }
    void Update(ResultScene& owner) const override {
        if (UpdateAutoConfirm(owner)) { return; }
        owner.UpdateContent();
        owner.UpdateMenu();
    }
};

class ResultScene::LeavingState final : public ResultScene::State {
public:
    bool IsLeaving() const override { return true; }
    void Update(ResultScene& owner) const override {
        owner.UpdateContent();
        ++owner.transitionTimer_;
        owner.UpdateMenu();
        if (owner.transitionTimer_ != kFadeDuration) { return; }
        // 遷移要求は暗転が終わった状態自身が一度だけ出す。
        if (owner.demoMode_) { owner.demoRestartReady_ = true; }
        else if (owner.buttonActions_[owner.selectedButton_] == Action::Restart) {
            owner.GetSceneManager()->SetNextScene(std::make_unique<GamePlayScene>());
        } else { owner.GetSceneManager()->SetNextScene(std::make_unique<TitleScene>()); }
        owner.ChangeState(GetState<FinishedState>());
    }
};

class ResultScene::FinishedState final : public ResultScene::State {
public:
    bool IsLeaving() const override { return true; }
    void Update(ResultScene& owner) const override {
        // 次シーンの読み込み待ちでは背景を保ち、遷移を再要求しない。
        owner.UpdateContent();
        ++owner.transitionTimer_;
        owner.UpdateMenu();
    }
};

template<class T>
const ResultScene::State& ResultScene::GetState() { static const T state; return state; }
ResultScene::ResultScene() : state_(&GetState<PresentingState>()) {}
void ResultScene::ChangeState(const State& state) { state_ = &state; }
bool ResultScene::IsLeaving() const { return state_->IsLeaving(); }
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
    ChangeState(GetState<PresentingState>());
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
    state_->OnReady(*this);
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
    if (!demoMode_ && state_->CanSelect() && menuReady) {
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
            state_->OnConfirm(*this);
        }
    }

    // 入力結果を渡し、画面ごとの選択演出を仮想関数で更新する。
    UpdateMenuVisuals(menuReady, hovered);
}

void ResultScene::Update() {
    ++frame_;
    state_->Update(*this);
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
