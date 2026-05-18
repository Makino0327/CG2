#pragma once
#include <vector>
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "../../engine/math/Math.h"
#include "../BaseScene.h"

class Sprite;
class Object3d;
class ParticleSystem;

class ClearScene : public BaseScene
{
public:
    // クリアシーンを初期化する
    void Initialize() override;

    // クリアシーンを更新する
    void Update() override;

    // クリアシーンを描画する
    void Draw() override;

    // クリアシーンを終了する
    void Finalize() override;

private:
    // 初期化済みかを管理する
    bool initialized_ = false;

    // 表示用スプライト
    std::vector<std::unique_ptr<Sprite>> sprites_;

    // 表示用3Dオブジェクト
    std::unique_ptr<Object3d> objA_;

    // パーティクル
    std::unique_ptr<ParticleSystem> particleSystem_;

    // ライト用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
};
