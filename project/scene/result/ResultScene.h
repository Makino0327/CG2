#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "../BaseScene.h"
#include "../../engine/math/Math.h"

class Object3d;
class Sprite;

// クリア画面とゲームオーバー画面で共通の、タイトルと同じ見た目のメニュー画面
// 背景は床だけ、上部に見出し、下部に白黒のボタンを並べる
class ResultScene : public BaseScene {
public:
    enum class Type { Clear, GameOver };

    explicit ResultScene(Type type);
    // 描画クラスの解放は、型の定義を読み込んだcpp側で行う
    ~ResultScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    // ボタンを決定したときの移動先
    enum class Action { Title, Restart };

    Sprite* CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color);
    Sprite* CreateLabel(const Vector2& textureLeftTop, const Vector2& textureSize,
        const Vector2& position, const Vector2& size);
    void CreateMenu();
    void UpdateMenu();

    Type type_;
    bool initialized_ = false;
    bool isLeaving_ = false;
    int frame_ = 0;
    int transitionTimer_ = 0;
    int selectedButton_ = 0;

    // 背景の床
    std::unique_ptr<Object3d> floor_;

    // 枠・文字・ボタン・フェードは通常のSpriteで描く
    std::vector<std::unique_ptr<Sprite>> uiSprites_;
    std::vector<Action> buttonActions_;
    std::vector<Sprite*> buttonBackgrounds_;
    std::vector<Sprite*> buttonLabels_;
    Sprite* fadeSprite_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
};
