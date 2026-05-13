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
    assert(context_.sound);
    assert(sceneManager_);

    // 3D オブジェクト共通
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(context_.object3dCommon);

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("cube.obj");

    // simpleSkin を読む
    ModelManager::GetInstance()->LoadModel("simpleSkin/simpleSkin.gltf");

    // human を読む
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");

    Animation humanWalkAnimation =
        LoadAnimationFile("Resources/human", "walk.gltf");

    // AnimatedCube のアニメーションを読み込む
    //Animation animation =
    //    LoadAnimationFile("Resources/AnimatedCube", "AnimatedCube.gltf");

    // テクスチャ
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle2.png");
    texMan->LoadTexture("Resources/fence.png");
    texMan->LoadTexture("Resources/Cube.png");
    texMan->LoadTexture("Resources/skybox.dds");
    texMan->LoadTexture("Resources/gradationLine.png"); // // Ring 用のグラデーションテクスチャ

    // // 通常パーティクル
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::CircleBurst);
    particleSystem_->SetMeshType(EffectMeshType::Plane);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // Ring パーティクル
    ringParticleSystem_ = std::make_unique<ParticleSystem>();
    ringParticleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::CircleBurst);
    ringParticleSystem_->SetMeshType(EffectMeshType::Ring);
    ringParticleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // Cylinder パーティクル
    cylinderParticleSystem_ = std::make_unique<ParticleSystem>();
    cylinderParticleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::CircleBurst);
    cylinderParticleSystem_->SetMeshType(EffectMeshType::Cylinder);
    cylinderParticleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // Material
    materialResource_ = context_.dxCommon->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->lightingType = 0;
    materialData->environmentCoefficient = 0.0f;
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

    // simpleSkin
    objA_ = std::make_unique<Object3d>();
    objA_->Initialize(context_.object3dCommon);
    objA_->SetModel("simpleSkin/simpleSkin.gltf");
    objA_->SetEnvironmentTexture("Resources/skybox.dds");
    objA_->SetEnvironmentCoefficient(0.35f);
    // 確認用の transform 初期値を適用する
    objA_->SetScale(objAScale_);
    objA_->SetRotate(objARotate_);
    objA_->SetTranslate(objATranslate_);

    // human
    objB_ = std::make_unique<Object3d>();
    objB_->Initialize(context_.object3dCommon);
    objB_->SetModel("human/walk.gltf");

    // human に walk animation を設定する
    objB_->SetAnimation(humanWalkAnimation);

    // animation 再生を開始する
    objB_->SetIsAnimationPlaying(isHumanAnimationPlaying_);

    objB_->SetEnvironmentTexture("Resources/skybox.dds");
    objB_->SetEnvironmentCoefficient(0.35f);
    // 確認用の transform 初期値を適用する
    objB_->SetScale(objBScale_);
    objB_->SetRotate(objBRotate_);
    objB_->SetTranslate(objBTranslate_);



	// Skybox
    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(context_.dxCommon, context_.srvManager);
    skyboxCommon_->SetDefaultCamera(context_.camera);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(skyboxCommon_.get());
    skybox_->SetCamera(context_.camera);


    seSelect_ = context_.sound->SoundLoad(L"./Resources/select.mp3");


    soundLoaded_ = true;

    // 線描画共通を初期化する
    line3dCommon_ = std::make_unique<Line3DCommon>();
    line3dCommon_->Initialize(context_.dxCommon, context_.srvManager);

    // simpleSkin 用の Skeleton デバッグ描画を初期化する
    debugSkeletonRendererA_ = std::make_unique<DebugSkeletonRenderer>();
    debugSkeletonRendererA_->Initialize(context_.dxCommon, line3dCommon_.get());

    // simpleSkin は今まで通りの大きさ
    debugSkeletonRendererA_->SetJointRadius(0.15f);

    // human 用の Skeleton デバッグ描画を初期化する
    debugSkeletonRendererB_ = std::make_unique<DebugSkeletonRenderer>();
    debugSkeletonRendererB_->Initialize(context_.dxCommon, line3dCommon_.get());

    // human は Joint 球を小さくする
    debugSkeletonRendererB_->SetJointRadius(0.01f);



}

void GamePlayScene::Update()
{
    //if (context_.sound) {
    //    context_.sound->Update();
    //}

    //if (soundLoaded_) {

    //    if (context_.input->TriggerKey(DIK_SPACE)) {
    //        context_.sound->SoundPlayWave(seSelect_);
    //    }
    //}
    const float dt = 1.0f / 60.0f;

    if (objA_) {
        // ImGui で変更した simpleSkin の transform を反映する
        objA_->SetScale(objAScale_);
        objA_->SetRotate(objARotate_);
        objA_->SetTranslate(objATranslate_);
    }

    if (objB_) {
        // ImGui で変更した human の transform を反映する
        objB_->SetScale(objBScale_);
        objB_->SetRotate(objBRotate_);
        objB_->SetTranslate(objBTranslate_);

        // human の animation 再生状態を反映する
        objB_->SetIsAnimationPlaying(isHumanAnimationPlaying_);
    }

    //for (auto& sprite : sprites_) {
    //    if (sprite) { sprite->Update(); }
    //}

    if (objA_) { objA_->Update(); }
    if (objB_) { objB_->Update(); }


    // ★ context_ 経由に変更
    if (context_.camera) { context_.camera->Update(); }


    if (skybox_) { skybox_->Update(); }

    // Particleを毎フレーム更新する
    if (particleSystem_) { particleSystem_->Update(dt); }

    // Ring パーティクルを更新する
    if (ringParticleSystem_) { ringParticleSystem_->Update(dt); }

    // Cylinder パーティクルを更新する
    if (cylinderParticleSystem_) { cylinderParticleSystem_->Update(dt); }

    //if (!sprites_.empty() && sprites_[0]) {
    //    sprites_[0]->SetPosition(spritePos_);
    //}
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
    //for (auto& sprite : sprites_) {
    //    if (sprite) { sprite->Draw(); }
    //}

    if (skybox_) {
        skybox_->Draw();
    }

    // 3D
    
    if (objA_) { objA_->Draw(); }
    if (objB_) { objB_->Draw(); }


    if (objA_ && objA_->HasSkeleton() && debugSkeletonRendererA_ && context_.camera) {
        Matrix4x4 objectWorldMatrix = MakeAffineMatrix(
            objA_->GetScale(),
            objA_->GetRotate(),
            objA_->GetTranslate()
        );

        // simpleSkin 用の線データを組み立てる
        debugSkeletonRendererA_->Build(
            objA_->GetSkeleton(),
            objectWorldMatrix,
            context_.camera->GetViewProjectionMatrix());

        // simpleSkin の Skeleton を描画する
        debugSkeletonRendererA_->Draw();
    }


    if (objB_ && objB_->HasSkeleton() && debugSkeletonRendererB_ && context_.camera) {
        Matrix4x4 objectWorldMatrix = MakeAffineMatrix(
            objB_->GetScale(),
            objB_->GetRotate(),
            objB_->GetTranslate()
        );

        debugSkeletonRendererB_->Build(
            objB_->GetSkeleton(),
            objectWorldMatrix,
            context_.camera->GetViewProjectionMatrix());

        debugSkeletonRendererB_->Draw();
    }





    // Particle
    context_.particleCommon->CommonDrawSetting();

    // Particleを描画する
    if (particleSystem_) { particleSystem_->Draw(); }
    // Ring パーティクルを描画する
    if (ringParticleSystem_) { ringParticleSystem_->Draw(); }
    // Cylinder パーティクルを描画する
    if (cylinderParticleSystem_) { cylinderParticleSystem_->Draw(); }
}

void GamePlayScene::Finalize()
{
    // ★ 追加：サウンドデータの解放
    if (context_.sound) {
        context_.sound->SoundUnload(&seSelect_);
    }

    sprites_.clear();
    objA_.reset();
    objB_.reset();

    debugSkeletonRendererA_.reset();
    debugSkeletonRendererB_.reset();
    line3dCommon_.reset();

    object3d_.reset();
    particleSystem_.reset();
    ringParticleSystem_.reset();
    // Cylinder パーティクルを解放する
    cylinderParticleSystem_.reset();

    skybox_.reset();
    skyboxCommon_.reset();

    materialResource_.Reset();
    directionalLightResource_.Reset();

    soundLoaded_ = false;
    initialized_ = false;
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
    ImGui::Text("Skybox and objects use this camera.");
    ImGui::End();

    if (objA_ && objA_->GetMaterial()) {
        ImGui::Begin("Environment");
        ImGui::SliderFloat("Reflection", &objA_->GetMaterial()->environmentCoefficient, 0.0f, 1.0f);
        ImGui::End();
    }

    if (objA_) {
        ImGui::Begin("SimpleSkin");

        // simpleSkin の位置を確認しやすくする
        ImGui::DragFloat3("Translate", &objATranslate_.x, 0.05f);

        // simpleSkin の回転を確認しやすくする
        ImGui::DragFloat3("Rotate", &objARotate_.x, 0.01f);

        // simpleSkin の拡大率を確認しやすくする
        ImGui::DragFloat3("Scale", &objAScale_.x, 0.01f);

        ImGui::End();
    }

    if (objB_) {
        ImGui::Begin("Human");

        // human の位置を確認しやすくする
        ImGui::DragFloat3("Translate", &objBTranslate_.x, 0.05f);

        // human の回転を確認しやすくする
        ImGui::DragFloat3("Rotate", &objBRotate_.x, 0.01f);

        // human の拡大率を確認しやすくする
        ImGui::DragFloat3("Scale", &objBScale_.x, 0.01f);

        // animation の ON/OFF を切り替えて bind pose と比較しやすくする
        ImGui::Checkbox("Play Animation", &isHumanAnimationPlaying_);

        // animation の時刻を最初に戻して見比べやすくする
        if (ImGui::Button("Reset Animation")) {
            objB_->ResetAnimationTime();
        }

        ImGui::End();
    }

    if (context_.offscreenRenderer) {
        ImGui::Begin("Post Effect");

        const char* items[] = {
     "Copy",
     "Grayscale",
     "Sepia",
     "Vignette",
     "BoxFilter",
     "GaussianFilter",
     "RadialBlur",
     "Dissolve",
     "DepthOutline",
        };





        int current =
            static_cast<int>(context_.offscreenRenderer->GetPostEffectType());

        if (ImGui::Combo("Effect", &current, items, IM_ARRAYSIZE(items))) {
            context_.offscreenRenderer->SetPostEffectType(
                static_cast<PostEffectType>(current));
        }

        if (context_.offscreenRenderer->GetPostEffectType() == PostEffectType::Dissolve) {
            int maskType = context_.offscreenRenderer->GetDissolveMaskType();
            const char* maskItems[] = {
                "noise0",
                "noise1",
            };

            if (ImGui::Combo("Mask", &maskType, maskItems, IM_ARRAYSIZE(maskItems))) {
                context_.offscreenRenderer->SetDissolveMaskType(maskType);
            }

            float durationSeconds = context_.offscreenRenderer->GetDissolveDuration();
            if (ImGui::DragFloat("Duration", &durationSeconds, 0.1f, 0.1f, 10.0f)) {
                context_.offscreenRenderer->SetDissolveDuration(durationSeconds);
            }

            float elapsedSeconds = context_.offscreenRenderer->GetDissolveElapsedTime();
            float maxSeconds = context_.offscreenRenderer->GetDissolveDuration();

            if (ImGui::SliderFloat("Current Time", &elapsedSeconds, 0.0f, maxSeconds)) {
                context_.offscreenRenderer->SetDissolveElapsedTime(elapsedSeconds);
            }


            if (ImGui::Button("Start")) {
                context_.offscreenRenderer->StartDissolve();
            }

            ImGui::Text("Playing: %s",
                context_.offscreenRenderer->IsDissolvePlaying() ? "true" : "false");
            ImGui::Text("Threshold: %.2f",
                context_.offscreenRenderer->GetDissolveThreshold());
        }

        ImGui::End();
    }
    // 通常パーティクルの ImGui
    if (particleSystem_) {
        ImGui::PushID("NormalParticle");
        particleSystem_->ShowImGui("Particle Editor##Normal");
        ImGui::PopID();
    }

    // Ring パーティクルの ImGui
    if (ringParticleSystem_) {
        ImGui::PushID("RingParticle");
        ringParticleSystem_->ShowImGui("Particle Editor##Ring");
        ImGui::PopID();
    }


    // Cylinder パーティクルの ImGui
    if (cylinderParticleSystem_) {
        ImGui::PushID("CylinderParticle");
        cylinderParticleSystem_->ShowImGui("Particle Editor##Cylinder");
        ImGui::PopID();
    }


#endif
}
