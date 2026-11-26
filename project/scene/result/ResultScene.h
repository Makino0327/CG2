#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <memory>
#include <vector>
#include <initializer_list>
#include <wrl.h>
#include <d3d12.h>
#include "../BaseScene.h"
#include "../../engine/math/Math.h"

class Sprite;

// ボス撃破時点で確定し、クリア画面へ値で渡す戦績。
struct ClearResult {
    double clearSeconds = 0.0;
    double bossSeconds = 0.0;
    int enemiesDefeated = 0;
    int damageTakenCount = 0;
    size_t shotsFired = 0;
    size_t shotsHit = 0;
};

// 操作と遷移を共通化し、画面固有の振る舞いを派生クラスへ任せる。
class ResultScene : public BaseScene {
public:
    ~ResultScene() override;
    void Initialize() override final;
    void Update() override final;
    void Draw() override final;
    void Finalize() override final;
    // デモの周回通知も、表示内容によらず同じ入口で扱う。
    void SetDemoMode(bool enabled) { demoMode_ = enabled; }
    bool IsDemoRestartReady() const { return demoRestartReady_; }

protected:
    enum class Action { Title, Restart };
    // 配置の違いは振る舞いではないため、設定データとして受け取る。
    struct MenuLayout { float x; float y; float width; float height; float spacing; };
    void ConfigureMenu(const MenuLayout& layout, std::initializer_list<Action> actions, int demoConfirmFrame);
    Sprite* CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color);
    Sprite* CreateLabel(const Vector2& textureLeftTop, const Vector2& textureSize,
        const Vector2& position, const Vector2& size, const char* texture);
    Sprite* CreateBackground(const Vector4& color);
    void UpdateSprites();
    ID3D12Resource* GetUiLightResource() const { return directionalLightResource_.Get(); }
    int GetFrame() const { return frame_; }
    int GetTransitionTimer() const { return transitionTimer_; }
    int GetSelectedButton() const { return selectedButton_; }
    size_t GetButtonCount() const { return buttonActions_.size(); }
    Action GetButtonAction(size_t index) const { return buttonActions_.at(index); }
    bool IsLeaving() const { return isLeaving_; }
    bool IsDemoMode() const { return demoMode_; }
    // 各画面で成立する処理だけを要求し、内部データは公開しない。
    virtual void CreateContent() = 0;
    virtual void UpdateContent() = 0;
    virtual bool IsMenuReady() const = 0;
    virtual void UpdateMenuVisuals(bool menuReady, int hovered) = 0;
    virtual void DrawBackground() = 0;
    virtual void FinalizeContent() = 0;

private:
    void UpdateMenu();
    bool initialized_ = false;
    bool demoMode_ = false;
    bool demoRestartReady_ = false;
    bool isLeaving_ = false;
    int frame_ = 0;
    int transitionTimer_ = 0;
    int selectedButton_ = 0;
    int demoConfirmFrame_ = 180;
    MenuLayout menuLayout_{};
    std::vector<std::unique_ptr<Sprite>> uiSprites_;
    std::vector<Action> buttonActions_;
    Sprite* background_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
};
