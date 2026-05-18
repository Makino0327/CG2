#include "ClearScene.h"

#include <cassert>
#include <memory>
#include <string>

#include "../../engine/base/DirectX/DirectXCommon.h"
#include "../../engine/base/srv/SrvManager.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/particle/ParticleCommon.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/3d/model/ModelManager.h"
#include "../../engine/2d/texture/TextureManager.h"
#include "../../engine/input/Input.h"

#include "../SceneManager.h"
#include "../title/TitleScene.h"

void ClearScene::Initialize()
{
    if (initialized_) {
        return;
    }
    initialized_ = true;

    // 必要な共通機能があるか確認する
    assert(context_.dxCommon);
    assert(context_.srvManager);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.particleCommon);
    assert(context_.input);

    // モデルを読み込む
    ModelManager::GetInstance()->LoadModel("fence.obj");

    // テクスチャを読み込む
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/circle.png");

    // ライトを作る
    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

    // スプライトを作る
    sprites_.clear();
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(context_.spriteCommon, directionalLightResource_.Get(), "Resources/monsterBall.png");
    sprite->SetPosition({ 300.0f, 200.0f });
    sprites_.push_back(std::move(sprite));

    // 3Dオブジェクトを作る
    objA_ = std::make_unique<Object3d>();
    objA_->Initialize(context_.object3dCommon);
    objA_->SetModel("fence.obj");
    objA_->SetTexture("Resources/circle.png");
    objA_->SetTranslate({ 0.0f, 0.0f, 0.0f });

    // パーティクルを作る
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::CircleBurst);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });
}

void ClearScene::Update()
{
    // スペースキーでタイトルへ戻る
    if (context_.input->TriggerKey(DIK_SPACE)) {
        sceneManager_->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    const float dt = 1.0f / 60.0f;

    // スプライトを更新する
    for (auto& sprite : sprites_) {
        if (sprite) {
            sprite->Update();
        }
    }

    // 3Dオブジェクトを更新する
    if (objA_) {
        objA_->Update();
    }

    // パーティクルを更新する
    if (particleSystem_) {
        particleSystem_->Update(dt);
    }
}

void ClearScene::Draw()
{
    assert(context_.dxCommon);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.particleCommon);

    // スプライトを描画する
    context_.spriteCommon->CommonDrawSetting();
    for (auto& sprite : sprites_) {
        if (sprite) {
            sprite->Draw();
        }
    }

    // 3Dを描画する
    context_.object3dCommon->CommonDrawSetting();
    if (objA_) {
        objA_->Draw();
    }

    // パーティクルを描画する
    context_.particleCommon->CommonDrawSetting();
    if (particleSystem_) {
        particleSystem_->Draw();
    }
}

void ClearScene::Finalize()
{
    // 使ったオブジェクトを解放する
    sprites_.clear();
    objA_.reset();
    particleSystem_.reset();
    directionalLightResource_.Reset();
    initialized_ = false;
}
