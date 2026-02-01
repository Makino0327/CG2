#pragma once

#include <vector>
#include <string>
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "Math.h"
#include "BaseScene.h"

// ===== 前方宣言 =====
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class Object3dCommon;
class ModelCommon;
class ParticleCommon;
class Camera;
class Input;
class SceneManager;

class Sprite;
class Object3d;
class ParticleSystem;

//==================================================
// GamePlayScene
//==================================================
class GamePlayScene : public BaseScene
{
public:
    // ===== BaseScene interface（SceneManagerから呼ばれる）=====
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

public:
    // ===== Game / TitleScene から呼ぶ =====
    // シーン生成直後に必ずこれを呼んでから SetNextScene する
    void SetContext(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        Object3dCommon* object3dCommon,
        ModelCommon* modelCommon,
        ParticleCommon* particleCommon,
        Camera* camera,
        Input* input,
        SceneManager* sceneManager
    );

private:
    // ===== 共通（Gameが所有・Sceneは参照だけ）=====
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;
    ModelCommon* modelCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Input* input_ = nullptr;
    SceneManager* sceneManager_ = nullptr;

    // ===== シーン固有（GamePlaySceneが所有）=====
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<Object3d> objA_;

    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    Vector2 spritePos_ = { 0.0f, 0.0f };

    bool initialized_ = false; // 二重初期化防止（保険）
};
