#include "TitleScene.h"

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
#include "Particle.h"   // ParticleSystem / ParticleType がここにある前提
#include "ModelCommon.h"
#include "ModelManager.h"
#include "TextureManager.h"

#include "SceneManager.h"
#include "Input.h"
#include "GamePlayScene.h"

//==================================================
// TitleScene
//==================================================

void TitleScene::SetContext(
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
// BaseScene interface
//==================================================

void TitleScene::Initialize()
{
    // 二重初期化防止（SceneManagerが切替時に1回だけ呼ぶ前提でも、保険）
    if (initialized_) { return; }
    initialized_ = true;

    assert(dxCommon_);
    assert(srvManager_);
    assert(spriteCommon_);
    assert(object3dCommon_);
    assert(modelCommon_);
    assert(particleCommon_);
    assert(camera_);
    assert(input_);
    assert(sceneManager_);

    // -------- モデル読み込み（Game側で Initialize 済み前提：ここは Load だけ） --------
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");

    // -------- テクスチャ（必要なら事前ロード） --------
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle.png");
    texMan->LoadTexture("Resources/fence.png");

    // -------- パーティクル --------
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(dxCommon_, particleCommon_, camera_, srvManager_, ParticleType::CircleBurst);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // -------- ライトCB（Spriteが受け取るやつ）--------
    directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

    // -------- スプライト --------
    sprites_.clear();
    sprites_.reserve(5);

    for (uint32_t i = 0; i < 5; ++i) {
        auto sprite = std::make_unique<Sprite>();

        std::string texPath = (i % 2 == 0)
            ? "Resources/uvChecker.png"
            : "Resources/monsterBall.png";

        sprite->Initialize(spriteCommon_, directionalLightResource_.Get(), texPath);

        Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
        sprite->SetPosition(pos);

        sprites_.push_back(std::move(sprite));
    }

    // -------- 3D --------
    objA_ = std::make_unique<Object3d>();
    objA_->Initialize(object3dCommon_);
    objA_->SetModel("fence.obj");
    objA_->SetTexture("Resources/circle.png");
    objA_->SetTranslate({ 0.0f, 0.0f, 0.0f });

    // ついでに初期位置（UI等でいじるならここ）
    spritePos_ = { 100.0f, 200.0f };
}

void TitleScene::Update()
{
    assert(input_);
    assert(sceneManager_);

    // ===== シーン切り替え（unique_ptr版）=====
    if (input_->TriggerKey(DIK_SPACE)) {

        auto next = std::make_unique<GamePlayScene>();

        next->SetContext(
            dxCommon_,
            srvManager_,
            spriteCommon_,
            object3dCommon_,
            modelCommon_,
            particleCommon_,
            camera_,
            input_,
            sceneManager_
        );

        // ★所有権移動（delete不要）
        sceneManager_->SetNextScene(std::move(next));
        return; // 切替予約したフレームはこれ以上触らないのが安全
    }

    // dt固定（まず動くのを優先）
    const float dt = 1.0f / 60.0f;

    // Sprite Update
    for (auto& sprite : sprites_) {
        if (sprite) { sprite->Update(); }
    }

    // 3D Update
    if (objA_) { objA_->Update(); }

    // Camera
    if (camera_) { camera_->Update(); }

    // Particle
    if (particleSystem_) { particleSystem_->Update(dt); }

    // 例：スプライト0だけ座標反映
    if (!sprites_.empty() && sprites_[0]) {
        sprites_[0]->SetPosition(spritePos_);
    }
}

void TitleScene::Draw()
{
    assert(dxCommon_);
    assert(spriteCommon_);
    assert(object3dCommon_);
    assert(particleCommon_);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);

    // ===== Sprite =====
    spriteCommon_->CommonDrawSetting();
    for (auto& sprite : sprites_) {
        if (sprite) { sprite->Draw(); }
    }

    // ===== 3D =====
    object3dCommon_->CommonDrawSetting();
    if (objA_) { objA_->Draw(); }

    // ===== Particle =====
    particleCommon_->CommonDrawSetting();
    if (particleSystem_) { particleSystem_->Draw(); }
}

void TitleScene::Finalize()
{
    // ★“解放”は unique_ptr / ComPtr がやる
    // ここでは「終了処理・切断」を行う（必要なら）

    // Scene固有オブジェクトを先に落とす（順序を明確にしたいなら reset）
    sprites_.clear();
    objA_.reset();
    particleSystem_.reset();

    // ComPtr
    directionalLightResource_.Reset();
    materialResource_.Reset();

    initialized_ = false;
}
