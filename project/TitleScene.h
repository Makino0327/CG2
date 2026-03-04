#pragma once
#include <vector>
#include <string>
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "Math.h"
#include "BaseScene.h"

// 借り物の前方宣言は SceneContext.h に移動するため、ここからは削除できます。
// シーン固有で使うクラスだけ前方宣言を残します。
class Sprite;
class Object3d;
class ParticleSystem;

class TitleScene : public BaseScene {
public:
    // ★ 長かった SetContext(...) は削除！(BaseScene 側で面倒を見ます)

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    bool initialized_ = false;

    // ===== 借り物（所有しない）=====
    // ★ ここにあった9個のポインタ群（dxCommon_ など）もすべて削除！
    // 代わりに BaseScene から継承した `context_` (例: context_.dxCommon) を使用します。

    // ===== シーン固有（TitleScene が所有する）=====
    std::unique_ptr<Object3d> objA_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    Vector2 spritePos_ = { 100.0f, 100.0f };
};