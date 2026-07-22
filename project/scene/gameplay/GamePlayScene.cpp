#include "GamePlayScene.h"

#include <cassert>
#include <string>
#include <memory>

#include "../engine/base/directX/DirectXCommon.h"
#include "../engine/base/srv/SrvManager.h"
#include "../engine/2d/sprite/SpriteCommon.h"
#include "../engine/2d/sprite/Sprite.h"

#include "../engine/3d/obj3d/Object3dCommon.h"
#include "../engine/3d/obj3d/Object3d.h"
#include "../game/camera/Camera.h"

#include "../engine/particle/ParticleCommon.h"
#include "../engine/particle/Particle.h"
#include "../engine/3d/model/ModelCommon.h"
#include "../engine/3d/model/ModelManager.h"
#include "../engine/2d/texture/TextureManager.h"

#include "../engine/3d/skybox/SkyboxCommon.h"
#include "../engine/3d/skybox/Skybox.h"

#include "../engine/base/offscreen/OffscreenRenderer.h"

#include "../engine/input/Input.h"
#include "../scene/SceneManager.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

void GamePlayScene::Initialize()
{
    if (initialized_) { return; }
    initialized_ = true;

    // シーンで使う共通クラスが渡されているか確認する
    assert(context_.dxCommon);
    assert(context_.srvManager);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.line3dCommon);
    assert(context_.modelCommon);
    assert(context_.particleCommon);
    assert(context_.camera);
    assert(context_.input);
    assert(context_.sound);
    assert(sceneManager_);

    // カメラを見やすい位置に固定する
    context_.camera->SetTranslate({ 0.349f, 1.352f, 8.837f });
    context_.camera->SetRotate({ 0.039f, 3.138f, 0.000f });
    context_.camera->Update();

    // human の歩きモデルとしゃがみ歩きモデルを読み込む
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");
    ModelManager::GetInstance()->LoadModel("human/sneakWalk.gltf");

    // 歩きアニメーションの human を左側に配置する
    walkHuman_ = std::make_unique<Object3d>();
    walkHuman_->Initialize(context_.object3dCommon);
    walkHuman_->SetModel("human/walk.gltf");
    walkHuman_->SetAnimation(LoadAnimationFile("Resources", "human/walk.gltf"));
    walkHuman_->SetIsAnimationPlaying(true);
    walkHuman_->SetTranslate({ -1.5f, 0.0f, 0.0f });
    walkHuman_->SetScale({ 1.0f, 1.0f, 1.0f });

    // しゃがみ歩きアニメーションの human を右側に配置する
    sneakHuman_ = std::make_unique<Object3d>();
    sneakHuman_->Initialize(context_.object3dCommon);
    sneakHuman_->SetModel("human/sneakWalk.gltf");
    sneakHuman_->SetAnimation(LoadAnimationFile("Resources", "human/sneakWalk.gltf"));
    sneakHuman_->SetIsAnimationPlaying(true);
    sneakHuman_->SetTranslate({ 1.5f, 0.0f, 0.0f });
    sneakHuman_->SetScale({ 1.0f, 1.0f, 1.0f });

    // 歩き human 用のデバッグスケルトン描画を初期化する
    walkSkeletonRenderer_ = std::make_unique<DebugSkeletonRenderer>();
    walkSkeletonRenderer_->Initialize(context_.dxCommon, context_.line3dCommon);
    walkSkeletonRenderer_->SetJointRadius(0.035f);

    // しゃがみ歩き human 用のデバッグスケルトン描画を初期化する
    sneakSkeletonRenderer_ = std::make_unique<DebugSkeletonRenderer>();
    sneakSkeletonRenderer_->Initialize(context_.dxCommon, context_.line3dCommon);
    sneakSkeletonRenderer_->SetJointRadius(0.035f);

    // テクスチャを読み込む
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle2.png");
    texMan->LoadTexture("Resources/fence.png");
    texMan->LoadTexture("Resources/Cube.png");
    texMan->LoadTexture("Resources/skybox.dds");
    texMan->LoadTexture("Resources/gradationLine.png");

    // マテリアル用の定数バッファを作る
    materialResource_ = context_.dxCommon->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->lightingType = 0;
    materialData->environmentCoefficient = 0.0f;
    materialData->uvTransform = MakeIdentity4x4();

    // ライト用の定数バッファを作る
    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

    // Skybox を初期化する
    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(context_.dxCommon, context_.srvManager);
    skyboxCommon_->SetDefaultCamera(context_.camera);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(skyboxCommon_.get());
    skybox_->SetCamera(context_.camera);

    // デバッグカメラを初期化する
    debugCamera_ = std::make_unique<DebugCamera>();
}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

    // Skybox を更新する
    if (skybox_) { skybox_->Update(); }

    // Particle を毎フレーム更新する
    if (particleSystem_) { particleSystem_->Update(dt); }

    // 歩き human のアニメーションと座標行列を更新する
    if (walkHuman_) { walkHuman_->Update(); }

    // しゃがみ歩き human のアニメーションと座標行列を更新する
    if (sneakHuman_) { sneakHuman_->Update(); }

    // デバッグモードのときだけカメラ操作を更新する
    if (debugCamera_ && context_.isDebugMode) {
        debugCamera_->Update(
            context_.camera,
            context_.input,
            context_.offscreenRenderer,
            *context_.isDebugMode);
    }

    // カメラ行列を更新する
    if (context_.camera) { context_.camera->Update(); }
}

void GamePlayScene::Draw()
{
    // 描画に必要な共通クラスがあるか確認する
    assert(context_.dxCommon);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.particleCommon);

    ID3D12GraphicsCommandList* commandList = context_.dxCommon->GetCommandList();
    assert(commandList);

    // Sprite の描画設定にする
    context_.spriteCommon->CommonDrawSetting();

    // Skybox を描画する
    if (skybox_) {
        skybox_->Draw();
    }

    // human 2体を描画する
    context_.object3dCommon->CommonDrawSetting();
    if (walkHuman_) { walkHuman_->Draw(); }
    if (sneakHuman_) { sneakHuman_->Draw(); }

    // human 2体分のデバッグスケルトンを描画する
    if (context_.camera && walkHuman_ && walkSkeletonRenderer_ && walkHuman_->HasSkeleton()) {
        Matrix4x4 walkWorldMatrix = MakeAffineMatrix(
            walkHuman_->GetScale(),
            walkHuman_->GetRotate(),
            walkHuman_->GetTranslate());
        walkSkeletonRenderer_->Build(
            walkHuman_->GetSkeleton(),
            walkWorldMatrix,
            context_.camera->GetViewProjectionMatrix());
        walkSkeletonRenderer_->Draw();
    }

    if (context_.camera && sneakHuman_ && sneakSkeletonRenderer_ && sneakHuman_->HasSkeleton()) {
        Matrix4x4 sneakWorldMatrix = MakeAffineMatrix(
            sneakHuman_->GetScale(),
            sneakHuman_->GetRotate(),
            sneakHuman_->GetTranslate());
        sneakSkeletonRenderer_->Build(
            sneakHuman_->GetSkeleton(),
            sneakWorldMatrix,
            context_.camera->GetViewProjectionMatrix());
        sneakSkeletonRenderer_->Draw();
    }

    // Particle の描画設定にする
    context_.particleCommon->CommonDrawSetting();

    // Particle を描画する
    if (particleSystem_) { particleSystem_->Draw(); }
}

void GamePlayScene::Finalize()
{
    sprites_.clear();

    walkHuman_.reset();
    sneakHuman_.reset();
    walkSkeletonRenderer_.reset();
    sneakSkeletonRenderer_.reset();
    particleSystem_.reset();
  
    skybox_.reset();
    skyboxCommon_.reset();

    materialResource_.Reset();
    directionalLightResource_.Reset();

    initialized_ = false;

    debugCamera_.reset();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    if (!context_.camera) {
        return;
    }

    Transform& cameraTransform = context_.camera->GetTransform();

    ImGui::Begin("Camera");
    ImGui::DragFloat3("Translate", &cameraTransform.translate.x, 0.1f);
    ImGui::DragFloat3("Rotate", &cameraTransform.rotate.x, 0.01f);
    ImGui::End();


    // オフスクリーン描画結果をデバッグ表示する
    if (context_.offscreenRenderer) {
        context_.offscreenRenderer->DrawDebugGameViewImGui();
        context_.offscreenRenderer->DrawImGui();
    }

#endif
}