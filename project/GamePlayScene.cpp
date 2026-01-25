#include "GamePlayScene.h"

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

void GamePlayScene::Initialize(DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    Object3dCommon* object3dCommon,
    ModelCommon* modelCommon,
    ParticleCommon* particleCommon,
    Camera* camera) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    object3dCommon_ = object3dCommon;
    modelCommon_ = modelCommon;
    particleCommon_ = particleCommon;
    camera_ = camera;

    // 3D オブジェクト（元コードにあったので移植）
    object3d_ = new Object3d();
    object3d_->Initialize(object3dCommon_);

    // 3Dモデルマネージャー（Loadだけ移植：InitializeはGame側でやる）
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");

    // （必要なら）テクスチャを事前ロード
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle.png");
    texMan->LoadTexture("Resources/fence.png");

    particleSystem_ = new ParticleSystem();
    particleSystem_->Initialize(dxCommon_, particleCommon_, camera_, srvManager_, ParticleType::CircleBurst);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });   // エミッタ基準位置

    // 通常モデル用のマテリアルリソースを作成
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->uvTransform = MakeIdentity4x4();

    // ライト用の定数バッファリソースを作成
    directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    directionalLightData->direction = Vector3(0.0f, -1.0f, 0.0f);
    directionalLightData->intensity = 4.0f;

    // Spriteの初期化
    for (uint32_t i = 0; i < 5; ++i) {
        Sprite* sprite = new Sprite();

        std::string texPath;
        if (i % 2 == 0) {
            texPath = "Resources/uvChecker.png";
        } else {
            texPath = "Resources/monsterBall.png";
        }

        sprite->Initialize(spriteCommon_, directionalLightResource_.Get(), texPath);

        Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
        sprite->SetPosition(pos);

        sprites_.push_back(sprite);
    }

    // Object3d を作る（元コードの objA）
    objA_ = new Object3d();
    objA_->Initialize(object3dCommon_);
    objA_->SetModel("fence.obj");
    objA_->SetTexture("Resources/circle.png");
    objA_->SetTranslate({ 0, 0, 0 });
}

void GamePlayScene::Update(float deltaTime) {

    // Sprite Update（元コード通り）
    for (Sprite* sprite : sprites_) {
        sprite->Update();
    }

    // 3D Update（元コード通り）
    objA_->Update();
    camera_->Update();
    particleSystem_->Update(deltaTime);

    // スプライトに反映（元コード通り）
    if (!sprites_.empty()) {
        sprites_[0]->SetPosition(spritePos_);
    }
}

void GamePlayScene::Draw() {

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    (void)commandList; // 元コードにあったので残す（未使用警告回避）

    // ===== スプライト =====
    spriteCommon_->CommonDrawSetting();

    for (Sprite* sprite : sprites_) {
        sprite->Draw();
    }

    // ======= Draw =======
    object3dCommon_->CommonDrawSetting();
    objA_->Draw();

    // そのあとインスタンス描画（Particle用PSO）
    particleCommon_->CommonDrawSetting();
    particleSystem_->Draw();
}

void GamePlayScene::Finalize() {

    // Sprite
    for (Sprite* sprite : sprites_) {
        delete sprite;
    }
    sprites_.clear();

    // 3D
    delete objA_; objA_ = nullptr;
    delete object3d_; object3d_ = nullptr;

    // Particle
    delete particleSystem_; particleSystem_ = nullptr;

    // ComPtrは自動解放
    materialResource_.Reset();
    directionalLightResource_.Reset();
}
