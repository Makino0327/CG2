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

// ★ 長かった SetContext の実装は丸ごと削除！

//==================================================
// BaseScene interface
//==================================================

void TitleScene::Initialize()
{
    // 二重初期化防止
    if (initialized_) { return; }
    initialized_ = true;

    // ★ assert もすべて context_ 経由に変更
    assert(context_.dxCommon);
    assert(context_.srvManager);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.modelCommon);
    assert(context_.particleCommon);
    assert(context_.camera);
    assert(context_.input);

    // -------- モデル読み込み --------
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");

    // -------- テクスチャ --------
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle.png");
    texMan->LoadTexture("Resources/fence.png");

    // -------- パーティクル --------
    particleSystem_ = std::make_unique<ParticleSystem>();
    // ★ 引数を context_ 経由に変更
    particleSystem_->Initialize(context_.dxCommon, context_.particleCommon, context_.camera, context_.srvManager, ParticleType::CircleBurst);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // -------- ライトCB --------
    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
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

        // ★ 引数を context_ 経由に変更
        sprite->Initialize(context_.spriteCommon, directionalLightResource_.Get(), texPath);

        Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
        sprite->SetPosition(pos);

        sprites_.push_back(std::move(sprite));
    }

    // -------- 3D --------
    objA_ = std::make_unique<Object3d>();
    // ★ 引数を context_ 経由に変更
    objA_->Initialize(context_.object3dCommon);
    objA_->SetModel("fence.obj");
    objA_->SetTexture("Resources/circle.png");
    objA_->SetTranslate({ 0.0f, 0.0f, 0.0f });

    // ついでに初期位置
    spritePos_ = { 100.0f, 200.0f };
}

void TitleScene::Update()
{
    // ★ 改善後のシーン遷移（バッチリです！）
    if (context_.input->TriggerKey(DIK_SPACE)) {
        sceneManager_->SetNextScene(std::make_unique<GamePlayScene>());
        return;
    }

    // dt固定
    const float dt = 1.0f / 60.0f;

    // Sprite Update
    for (auto& sprite : sprites_) {
        if (sprite) { sprite->Update(); }
    }

    // 3D Update
    if (objA_) { objA_->Update(); }

    // Camera (★ context_ 経由に変更)
    if (context_.camera) { context_.camera->Update(); }

    // Particle
    if (particleSystem_) { particleSystem_->Update(dt); }

    // 例：スプライト0だけ座標反映
    if (!sprites_.empty() && sprites_[0]) {
        sprites_[0]->SetPosition(spritePos_);
    }
}

void TitleScene::Draw()
{
    // ★ すべて context_ 経由に変更
    assert(context_.dxCommon);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.particleCommon);

    ID3D12GraphicsCommandList* commandList = context_.dxCommon->GetCommandList();
    assert(commandList);

    // ===== Sprite =====
    context_.spriteCommon->CommonDrawSetting();
    for (auto& sprite : sprites_) {
        if (sprite) { sprite->Draw(); }
    }

    // ===== 3D =====
    context_.object3dCommon->CommonDrawSetting();
    if (objA_) { objA_->Draw(); }

    // ===== Particle =====
    context_.particleCommon->CommonDrawSetting();
    if (particleSystem_) { particleSystem_->Draw(); }
}

void TitleScene::Finalize()
{
    // ここは変更なしでOK
    sprites_.clear();
    objA_.reset();
    particleSystem_.reset();

    directionalLightResource_.Reset();
    materialResource_.Reset();

    initialized_ = false;
}