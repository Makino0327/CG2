#include "GamePlayScene.h"

#include <cassert>
#include <string>

#include "DirectXCommon.h"
#include "SrvManager.h"
#include "SpriteCommon.h"
#include "Sprite.h"

#include "Object3dCommon.h"
#include "Object3d.h"
#include "Camera.h"

#include "ParticleCommon.h"
#include "Particle.h"            // ParticleSystem / ParticleType がここにある想定
#include "ModelCommon.h"
#include "ModelManager.h"
#include "TextureManager.h"

#include "Input.h"
#include "SceneManager.h"

//==================================================
// 共通ポインタ受け取り（TitleSceneと同じ方式）
//==================================================
void GamePlayScene::SetContext(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    Object3dCommon* object3dCommon,
    ModelCommon* modelCommon,
    ParticleCommon* particleCommon,
    Camera* camera,
    Input* input,
    SceneManager* sceneManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    object3dCommon_ = object3dCommon;
    modelCommon_ = modelCommon;
    particleCommon_ = particleCommon;
    camera_ = camera;
    input_ = input;
    sceneManager_ = sceneManager;
}

//==================================================
// BaseScene interface（引数なし）
//==================================================
void GamePlayScene::Initialize()
{
    // ★ここで必ず初期化する（SceneManagerはこれしか呼ばない）
    assert(dxCommon_);
    assert(srvManager_);
    assert(spriteCommon_);
    assert(object3dCommon_);
    assert(modelCommon_);
    assert(particleCommon_);
    assert(camera_);
    assert(input_);
    assert(sceneManager_);

    // 3D オブジェクト共通（必要なら）
    object3d_ = new Object3d();
    object3d_->Initialize(object3dCommon_);

    // モデル読み込み（Game側で ModelManager::Initialize 済み前提、ここはLoad）
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");

    // テクスチャ（必要なら）
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle.png");
    texMan->LoadTexture("Resources/fence.png");

    // Particle
    particleSystem_ = new ParticleSystem();
    particleSystem_->Initialize(dxCommon_, particleCommon_, camera_, srvManager_, ParticleType::CircleBurst);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // Material（使ってないなら消してもOK）
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->uvTransform = MakeIdentity4x4();

    // DirectionalLight（Spriteが受け取る）
    directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

    // Sprites
    sprites_.clear();
    sprites_.reserve(5);
    for (uint32_t i = 0; i < 5; ++i) {
        Sprite* sprite = new Sprite();

        std::string texPath = (i % 2 == 0)
            ? "Resources/uvChecker.png"
            : "Resources/monsterBall.png";

        sprite->Initialize(spriteCommon_, directionalLightResource_.Get(), texPath);

        Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
        sprite->SetPosition(pos);

        sprites_.push_back(sprite);
    }

    // 3D
    objA_ = new Object3d();
    objA_->Initialize(object3dCommon_);
    objA_->SetModel("fence.obj");
    objA_->SetTexture("Resources/circle.png");
    objA_->SetTranslate({ 0.0f, 0.0f, 0.0f });

    
}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

   
    for (Sprite* sprite : sprites_) {
        if (sprite) { sprite->Update(); }
    }

    if (objA_) { objA_->Update(); }
    if (camera_) { camera_->Update(); }
    if (particleSystem_) { particleSystem_->Update(dt); }

    if (!sprites_.empty() && sprites_[0]) {
        sprites_[0]->SetPosition(spritePos_);
    }
}

void GamePlayScene::Draw()
{
    assert(dxCommon_);
    assert(spriteCommon_);
    assert(object3dCommon_);
    assert(particleCommon_);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);

    // Sprite
    spriteCommon_->CommonDrawSetting();
    for (Sprite* sprite : sprites_) {
        if (sprite) { sprite->Draw(); }
    }

    // 3D
    object3dCommon_->CommonDrawSetting();
    if (objA_) { objA_->Draw(); }

    // Particle
    particleCommon_->CommonDrawSetting();
    if (particleSystem_) { particleSystem_->Draw(); }
}

void GamePlayScene::Finalize()
{
    for (Sprite* sprite : sprites_) {
        delete sprite;
    }
    sprites_.clear();

    delete objA_;
    objA_ = nullptr;

    delete object3d_;
    object3d_ = nullptr;

    delete particleSystem_;
    particleSystem_ = nullptr;

    materialResource_.Reset();
    directionalLightResource_.Reset();
}
