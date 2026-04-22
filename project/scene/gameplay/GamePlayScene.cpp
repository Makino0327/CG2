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

    // テクスチャ
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle2.png");
    texMan->LoadTexture("Resources/fence.png");
    texMan->LoadTexture("Resources/Cube.png");
    texMan->LoadTexture("Resources/skybox.dds");

    // Particle
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(context_.dxCommon, context_.particleCommon, context_.camera, context_.srvManager, ParticleType::CircleBurst);
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

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

    // 3D
    objA_ = std::make_unique<Object3d>();
    objA_->Initialize(context_.object3dCommon);
    objA_->SetModel("cube.obj");
    objA_->SetTexture("Resources/Cube.png");
    objA_->SetEnvironmentTexture("Resources/skybox.dds");
    objA_->SetEnvironmentCoefficient(0.35f);
    objA_->SetScale({ 1.5f, 1.5f, 1.5f });
    objA_->SetTranslate({ 0.0f, -5.0f, 20.0f });

	// Skybox
    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(context_.dxCommon, context_.srvManager);
    skyboxCommon_->SetDefaultCamera(context_.camera);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(skyboxCommon_.get());
    skybox_->SetCamera(context_.camera);


    seSelect_ = context_.sound->SoundLoad(L"./Resources/select.mp3");


    soundLoaded_ = true;
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

    //for (auto& sprite : sprites_) {
    //    if (sprite) { sprite->Update(); }
    //}

    if (objA_) { objA_->Update(); }

    // ★ context_ 経由に変更
    if (context_.camera) { context_.camera->Update(); }


    if (skybox_) { skybox_->Update(); }

    // Particleを毎フレーム更新する
    if (particleSystem_) { particleSystem_->Update(dt); }

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
    context_.object3dCommon->CommonDrawSetting();
    if (objA_) { objA_->Draw(); }

    // Particle
    context_.particleCommon->CommonDrawSetting();

    // Particleを描画する
    if (particleSystem_) { particleSystem_->Draw(); }

}

void GamePlayScene::Finalize()
{
    // ★ 追加：サウンドデータの解放
    if (context_.sound) {
        context_.sound->SoundUnload(&seSelect_);
    }

    sprites_.clear();
    objA_.reset();
    object3d_.reset();
    particleSystem_.reset();

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
    if (context_.offscreenRenderer) {
        ImGui::Begin("Post Effect");

        const char* items[] = {
            "Copy",
            "Grayscale",
            "Sepia",
        };

        int current =
            static_cast<int>(context_.offscreenRenderer->GetPostEffectType());

        if (ImGui::Combo("Effect", &current, items, IM_ARRAYSIZE(items))) {
            context_.offscreenRenderer->SetPostEffectType(
                static_cast<PostEffectType>(current));
        }

        ImGui::End();
    }
    // Particleの調整用ImGuiを表示する
    if (particleSystem_) {
        particleSystem_->ShowImGui();
    }

#endif
}
