#include "GamePlayScene.h"

#include <cassert>
#include <string>
#include <memory>

#include "DirectXCommon.h"
#include "SrvManager.h"
#include "SpriteCommon.h"
#include "Sprite.h"

#include "Object3dCommon.h"
#include "Object3d.h"
#include "Camera.h"

#include "ParticleCommon.h"
#include "Particle.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "TextureManager.h"

#include "Input.h"
#include "SceneManager.h"

//==================================================
// ★ SetContext は BaseScene 側で処理されるため削除！
//==================================================

//==================================================
// BaseScene interface
//==================================================
void GamePlayScene::Initialize()
{
    if (initialized_) { return; }
    initialized_ = true;

    // ★ すべて context_ 経由に変更。sceneManager_ は直接使用。
    assert(context_.dxCommon);
    assert(context_.srvManager);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.modelCommon);
    assert(context_.particleCommon);
    assert(context_.camera);
    assert(context_.input);
    assert(sceneManager_);

    // 3D オブジェクト共通
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(context_.object3dCommon);

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");

    // テクスチャ
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle.png");
    texMan->LoadTexture("Resources/fence.png");

    // Particle
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(context_.dxCommon, context_.particleCommon, context_.camera, context_.srvManager, ParticleType::CircleBurst);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // Material
    materialResource_ = context_.dxCommon->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->uvTransform = MakeIdentity4x4();

    // DirectionalLight
    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

    // Sprites
    sprites_.clear();
    sprites_.reserve(5);

    for (uint32_t i = 0; i < 5; ++i) {
        auto sprite = std::make_unique<Sprite>();

        std::string texPath = (i % 2 == 0)
            ? "Resources/uvChecker.png"
            : "Resources/monsterBall.png";

        sprite->Initialize(context_.spriteCommon, directionalLightResource_.Get(), texPath);

        Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
        sprite->SetPosition(pos);

        sprites_.push_back(std::move(sprite));
    }

    // 3D
    objA_ = std::make_unique<Object3d>();
    objA_->Initialize(context_.object3dCommon);
    objA_->SetModel("fence.obj");
    objA_->SetTexture("Resources/circle.png");
    objA_->SetTranslate({ 0.0f, 0.0f, 0.0f });
}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

    for (auto& sprite : sprites_) {
        if (sprite) { sprite->Update(); }
    }

    if (objA_) { objA_->Update(); }

    // ★ context_ 経由に変更
    if (context_.camera) { context_.camera->Update(); }
    if (particleSystem_) { particleSystem_->Update(dt); }

    if (!sprites_.empty() && sprites_[0]) {
        sprites_[0]->SetPosition(spritePos_);
    }
}

void GamePlayScene::Draw()
{
    // ★ context_ 経由に変更
    assert(context_.dxCommon);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.particleCommon);

    ID3D12GraphicsCommandList* commandList = context_.dxCommon->GetCommandList();
    assert(commandList);

    // Sprite
    context_.spriteCommon->CommonDrawSetting();
    for (auto& sprite : sprites_) {
        if (sprite) { sprite->Draw(); }
    }

    // 3D
    context_.object3dCommon->CommonDrawSetting();
    if (objA_) { objA_->Draw(); }

    // Particle
    context_.particleCommon->CommonDrawSetting();
    if (particleSystem_) { particleSystem_->Draw(); }
}

void GamePlayScene::Finalize()
{
    sprites_.clear();
    objA_.reset();
    object3d_.reset();
    particleSystem_.reset();

    materialResource_.Reset();
    directionalLightResource_.Reset();

    initialized_ = false;
}