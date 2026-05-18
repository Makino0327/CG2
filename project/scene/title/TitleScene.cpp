#include "TitleScene.h"

#include <cassert>
#include <string>
#include <memory>

#include "../../engine/base/DirectX/DirectXCommon.h"
#include "../../engine/base/srv/SrvManager.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/2d/sprite/Sprite.h"

#include "../engine/3d/obj3d/Object3dCommon.h"
#include "../engine/3d/obj3d/Object3d.h"
#include "../game/camera/Camera.h"

#include "../engine/particle/ParticleCommon.h"
#include "../engine/particle/Particle.h"
#include "../engine/3d/model/ModelCommon.h"
#include "../engine/3d/model/ModelManager.h"
#include "../engine/2d/texture/TextureManager.h"

#include "../SceneManager.h"
#include "../../engine/input/Input.h"
#include "../gameplay/GamePlayScene.h"

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


    // -------- ライトCB --------
    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

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

    // Camera (★ context_ 経由に変更)
    if (context_.camera) { context_.camera->Update(); }
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


    // ===== 3D =====
    context_.object3dCommon->CommonDrawSetting();
 
    // ===== Particle =====
    context_.particleCommon->CommonDrawSetting();
}

void TitleScene::Finalize()
{
    directionalLightResource_.Reset();
    materialResource_.Reset();

    initialized_ = false;
}