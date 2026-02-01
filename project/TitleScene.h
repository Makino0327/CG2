#pragma once
#include <vector>
#include <string>
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "Math.h"
#include "BaseScene.h"

class DirectXCommon;
class SrvManager;
class SpriteCommon;
class Object3dCommon;
class ModelCommon;
class ParticleCommon;
class Camera;
class Sprite;
class Object3d;
class ParticleSystem;
class Input;
class SceneManager;

class TitleScene : public BaseScene {
public:
    void SetContext(DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        Object3dCommon* object3dCommon,
        ModelCommon* modelCommon,
        ParticleCommon* particleCommon,
        Camera* camera,
        Input* input,
        SceneManager* sceneManager);

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    bool initialized_ = false;

    // ===== 借り物（所有しない）=====
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;
    ModelCommon* modelCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Input* input_ = nullptr;
    SceneManager* sceneManager_ = nullptr;

    // ===== シーン固有（TitleScene が所有する）=====
    std::unique_ptr<Object3d> objA_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    Vector2 spritePos_ = { 100.0f, 100.0f };
};
