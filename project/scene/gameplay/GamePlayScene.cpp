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
    ModelManager::GetInstance()->LoadModel("multiMesh.obj");
    ModelManager::GetInstance()->LoadModel("multiMaterial.obj");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");
    ModelManager::GetInstance()->LoadModel("weapon/sword.obj");

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

    // MultiMeshとMultiMaterialの評価確認用モデルを並べて表示する
    multiMeshObject_ = std::make_unique<Object3d>();
    multiMeshObject_->Initialize(context_.object3dCommon);
    multiMeshObject_->SetModel("multiMesh.obj");
    multiMeshObject_->SetTranslate({ -3.0f, 0.0f, 0.0f });
    multiMeshObject_->SetScale({ 0.7f, 0.7f, 0.7f });

    multiMaterialObject_ = std::make_unique<Object3d>();
    multiMaterialObject_->Initialize(context_.object3dCommon);
    multiMaterialObject_->SetModel("multiMaterial.obj");
    multiMaterialObject_->SetTranslate({ 1.0f, 0.0f, 0.0f });
    multiMaterialObject_->SetScale({ 0.7f, 0.7f, 0.7f });
    // 手からパーティクルを出すためのスキニング済みhumanモデルを用意する
    humanObject_ = std::make_unique<Object3d>();
    humanObject_->Initialize(context_.object3dCommon);
    humanObject_->SetModel("human/walk.gltf");
    humanObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    humanObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    humanAnimation_ = LoadAnimationFile("Resources/human", "walk.gltf");
    humanObject_->SetAnimation(humanAnimation_);
    humanObject_->SetIsAnimationPlaying(true);

    // 右手に持たせるデバッグ用の剣モデルを作る
    weaponObject_ = std::make_unique<Object3d>();
    weaponObject_->Initialize(context_.object3dCommon);
    weaponObject_->SetModel("weapon/sword.obj");
    weaponObject_->SetScale(weaponScale_);
    weaponObject_->SetRotate(weaponRotate_);
    weaponObject_->SetTranslate(rightHandPosition_);

    // 手のJoint位置から手動Emitするパーティクルシステムを作る
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::CircleBurst);
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

	// Skybox
    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(context_.dxCommon, context_.srvManager);
    skyboxCommon_->SetDefaultCamera(context_.camera);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(skyboxCommon_.get());
    skybox_->SetCamera(context_.camera);

    // デバッグ画面でカメラを操作するクラスを生成する
    debugCamera_ = std::make_unique<DebugCamera>();

}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

    // Skyboxを毎フレーム更新する
    if (skybox_) { skybox_->Update(); }

    // デバッグモード中はDebugCameraでカメラを操作する
    if (debugCamera_ && context_.isDebugMode) {
        debugCamera_->Update(
            context_.camera,
            context_.input,
            context_.offscreenRenderer,
            *context_.isDebugMode);
    }

    if (context_.camera) { context_.camera->Update(); }

    // humanを先に更新して、最新の手のJoint位置を使えるようにする
    if (humanObject_) { humanObject_->Update(); }

    // human更新後の最新Joint位置を、パーティクルと武器の両方で使う
    UpdateHandJointPositions();
    UpdateWeaponTransform();

    // MultiMesh/MultiMaterial確認用モデルの行列を更新する
    if (multiMeshObject_) { multiMeshObject_->Update(); }
    if (multiMaterialObject_) { multiMaterialObject_->Update(); }

    // 手のJoint位置からパーティクルを発生させる
    EmitHandParticles(dt);

    // 発生済みパーティクルを更新する
    if (particleSystem_) { particleSystem_->Update(dt); }
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

    if (skybox_) {
        skybox_->Draw();
    }


    // MultiMesh/MultiMaterial確認用モデルを描画する
    context_.object3dCommon->CommonDrawSetting();
    if (showWeaponObject_ && weaponObject_) { weaponObject_->Draw(); }
    if (showHumanObject_ && humanObject_) { humanObject_->Draw(); }
    if (showMultiMeshMaterialDemo_) {
        if (multiMeshObject_) { multiMeshObject_->Draw(); }
        if (multiMaterialObject_) { multiMaterialObject_->Draw(); }
    }

    // Particle
    context_.particleCommon->CommonDrawSetting();

    // Particleを描画する
    if (particleSystem_) { particleSystem_->Draw(); }
    
}

void GamePlayScene::Finalize()
{
    sprites_.clear();

    object3d_.reset();
    humanObject_.reset();
    weaponObject_.reset();
    multiMeshObject_.reset();
    multiMaterialObject_.reset();
    particleSystem_.reset();
  
    skybox_.reset();
    skyboxCommon_.reset();

    materialResource_.Reset();
    directionalLightResource_.Reset();

    initialized_ = false;

    debugCamera_.reset();

}

bool GamePlayScene::GetJointWorldPosition(const std::string& jointName, Vector3& worldPosition) const
{
    if (!humanObject_ || !humanObject_->HasSkeleton()) {
        return false;
    }

    const Skeleton& skeleton = humanObject_->GetSkeleton();
    auto jointIt = skeleton.jointMap.find(jointName);
    if (jointIt == skeleton.jointMap.end()) {
        return false;
    }

    Matrix4x4 humanWorldMatrix = MakeAffineMatrix(
        humanObject_->GetScale(),
        humanObject_->GetRotate(),
        humanObject_->GetTranslate());

    // JointのSkeleton空間行列にモデルのWorld行列を掛けて、手のワールド座標を取り出す
    const Joint& joint = skeleton.joints[jointIt->second];
    Matrix4x4 jointWorldMatrix = Multiply(joint.skeletonSpaceMatrix, humanWorldMatrix);
    worldPosition = {
        jointWorldMatrix.m[3][0],
        jointWorldMatrix.m[3][1],
        jointWorldMatrix.m[3][2]
    };
    return true;
}

bool GamePlayScene::GetJointWorldMatrix(const std::string& jointName, Matrix4x4& worldMatrix) const
{
    if (!humanObject_ || !humanObject_->HasSkeleton()) {
        return false;
    }

    const Skeleton& skeleton = humanObject_->GetSkeleton();
    auto jointIt = skeleton.jointMap.find(jointName);
    if (jointIt == skeleton.jointMap.end()) {
        return false;
    }

    Matrix4x4 humanWorldMatrix = MakeAffineMatrix(
        humanObject_->GetScale(),
        humanObject_->GetRotate(),
        humanObject_->GetTranslate());

    // JointのSkeleton空間行列にhumanのWorld行列を掛けて、JointのWorld行列を作る
    const Joint& joint = skeleton.joints[jointIt->second];
    worldMatrix = Multiply(joint.skeletonSpaceMatrix, humanWorldMatrix);

    // Joint行列にスケールが混じると武器が消えるので、回転軸だけ正規化する
    Vector3 xAxis = Normalize(Vector3{ worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2] });
    Vector3 yAxis = Normalize(Vector3{ worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2] });
    Vector3 zAxis = Normalize(Vector3{ worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] });
    worldMatrix.m[0][0] = xAxis.x;
    worldMatrix.m[0][1] = xAxis.y;
    worldMatrix.m[0][2] = xAxis.z;
    worldMatrix.m[1][0] = yAxis.x;
    worldMatrix.m[1][1] = yAxis.y;
    worldMatrix.m[1][2] = yAxis.z;
    worldMatrix.m[2][0] = zAxis.x;
    worldMatrix.m[2][1] = zAxis.y;
    worldMatrix.m[2][2] = zAxis.z;
    return true;
}
bool GamePlayScene::GetGripSocketWorldMatrix(Matrix4x4& worldMatrix) const
{
    // 中指付け根の向きを使い、位置だけ手首・指・親指の中間へ寄せた仮想WeaponSocketを作る
    if (!GetJointWorldMatrix("mixamorig:RightHandMiddle1", worldMatrix) &&
        !GetJointWorldMatrix("RightHandMiddle1", worldMatrix)) {
        return false;
    }

    Vector3 handPosition{};
    Vector3 indexPosition{};
    Vector3 middlePosition{};
    Vector3 ringPosition{};
    Vector3 thumbPosition{};

    bool hasHand = GetJointWorldPosition("mixamorig:RightHand", handPosition);
    if (!hasHand) { hasHand = GetJointWorldPosition("RightHand", handPosition); }

    bool hasIndex = GetJointWorldPosition("mixamorig:RightHandIndex1", indexPosition);
    if (!hasIndex) { hasIndex = GetJointWorldPosition("RightHandIndex1", indexPosition); }

    bool hasMiddle = GetJointWorldPosition("mixamorig:RightHandMiddle1", middlePosition);
    if (!hasMiddle) { hasMiddle = GetJointWorldPosition("RightHandMiddle1", middlePosition); }

    bool hasRing = GetJointWorldPosition("mixamorig:RightHandRing1", ringPosition);
    if (!hasRing) { hasRing = GetJointWorldPosition("RightHandRing1", ringPosition); }

    bool hasThumb = GetJointWorldPosition("mixamorig:RightHandThumb1", thumbPosition);
    if (!hasThumb) { hasThumb = GetJointWorldPosition("RightHandThumb1", thumbPosition); }

    if (hasHand && hasIndex && hasMiddle && hasRing && hasThumb) {
        // 指の付け根と親指の間へ置くと、手首より「握っている」位置に見えやすい
        worldMatrix.m[3][0] = handPosition.x * 0.25f + indexPosition.x * 0.20f + middlePosition.x * 0.25f + ringPosition.x * 0.15f + thumbPosition.x * 0.15f;
        worldMatrix.m[3][1] = handPosition.y * 0.25f + indexPosition.y * 0.20f + middlePosition.y * 0.25f + ringPosition.y * 0.15f + thumbPosition.y * 0.15f;
        worldMatrix.m[3][2] = handPosition.z * 0.25f + indexPosition.z * 0.20f + middlePosition.z * 0.25f + ringPosition.z * 0.15f + thumbPosition.z * 0.15f;
    }

    return true;
}
void GamePlayScene::UpdateHandJointPositions()
{
    // Mixamo名と短い名前の両方を探して、モデル差し替え時にも手を拾えるようにする
    leftHandFound_ = GetJointWorldPosition("mixamorig:LeftHand", leftHandPosition_);
    if (!leftHandFound_) {
        leftHandFound_ = GetJointWorldPosition("LeftHand", leftHandPosition_);
    }

    rightHandFound_ = GetGripSocketWorldMatrix(rightHandWorldMatrix_);
    if (rightHandFound_) {
        rightHandPosition_ = {
            rightHandWorldMatrix_.m[3][0],
            rightHandWorldMatrix_.m[3][1],
            rightHandWorldMatrix_.m[3][2]
        };
    }
}

void GamePlayScene::UpdateWeaponTransform()
{
    if (!weaponObject_) {
        return;
    }

    if (attachWeaponToHand_ && rightHandFound_) {
        // WeaponSocket相当のローカル補正を作り、右手JointのWorld行列へ親子付けする
        Matrix4x4 weaponLocalMatrix = MakeAffineMatrix(weaponScale_, weaponRotate_, weaponOffset_);
        Matrix4x4 weaponWorldMatrix = reverseWeaponMatrixOrder_
            ? Multiply(rightHandWorldMatrix_, weaponLocalMatrix)
            : Multiply(weaponLocalMatrix, rightHandWorldMatrix_);
        weaponObject_->SetWorldMatrix(weaponWorldMatrix);
    } else {
        // 行列接続を切ったときは、右手位置 + offset の確認用表示に戻す
        weaponObject_->ClearWorldMatrix();
        if (rightHandFound_) {
            weaponObject_->SetTranslate({
                rightHandPosition_.x + weaponOffset_.x,
                rightHandPosition_.y + weaponOffset_.y,
                rightHandPosition_.z + weaponOffset_.z
            });
        } else {
            weaponObject_->SetTranslate(rightHandPosition_);
        }
        weaponObject_->SetRotate(weaponRotate_);
        weaponObject_->SetScale(weaponScale_);
    }

    weaponObject_->Update();
}

void GamePlayScene::EmitHandParticles(float deltaTime)
{
    if (!particleSystem_ || !emitHandParticles_) {
        return;
    }

    handParticleTimer_ += deltaTime;
    if (handParticleTimer_ < handParticleInterval_) {
        return;
    }
    handParticleTimer_ = 0.0f;


    // 手から出ていることが見えやすいように、少し上向きで短命の粒を出す
    if (emitLeftHandParticles_ && leftHandFound_) {
        particleSystem_->Emit(
            leftHandPosition_,
            { 0.12f, 0.12f, 0.12f },
            { -0.25f, 1.2f, 0.0f },
            { 0.35f, 0.75f, 1.0f, 1.0f },
            0.6f);
    }

    if (emitRightHandParticles_ && rightHandFound_) {
        particleSystem_->Emit(
            rightHandPosition_,
            { 0.12f, 0.12f, 0.12f },
            { 0.25f, 1.2f, 0.0f },
            { 1.0f, 0.65f, 0.25f, 1.0f },
            0.6f);
    }
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
    ImGui::Begin("Multi Mesh / Material");
    // 評価確認用モデルの表示を切り替える
    ImGui::Checkbox("Show Demo Models", &showMultiMeshMaterialDemo_);
    ImGui::Text("multiMesh.obj : Plane + Cube");
    ImGui::Text("multiMaterial.obj : uvChecker + monsterBall");
    ImGui::End();
    ImGui::Begin("Hand Particle");
    // human表示と手パーティクルのON/OFFを切り替える
    ImGui::Checkbox("Show Human", &showHumanObject_);
    ImGui::Checkbox("Emit From Hands", &emitHandParticles_);
    ImGui::Checkbox("Left Hand", &emitLeftHandParticles_);
    ImGui::Checkbox("Right Hand", &emitRightHandParticles_);
    ImGui::SliderFloat("Emit Interval", &handParticleInterval_, 0.01f, 0.3f, "%.2f");
    ImGui::Separator();
    ImGui::Checkbox("Show Weapon", &showWeaponObject_);
    ImGui::Checkbox("Attach To Hand", &attachWeaponToHand_);
    ImGui::Checkbox("Reverse Matrix Order", &reverseWeaponMatrixOrder_);
    ImGui::DragFloat3("Weapon Offset", &weaponOffset_.x, 0.01f);
    ImGui::DragFloat3("Weapon Rotate", &weaponRotate_.x, 0.01f);
    ImGui::DragFloat3("Weapon Scale", &weaponScale_.x, 0.01f, 0.01f, 2.0f);
    ImGui::Text("Left  %.2f %.2f %.2f", leftHandPosition_.x, leftHandPosition_.y, leftHandPosition_.z);
    ImGui::Text("Right %.2f %.2f %.2f", rightHandPosition_.x, rightHandPosition_.y, rightHandPosition_.z);
    ImGui::End();

    if (particleSystem_) {
        particleSystem_->ShowImGui("Hand Particle Detail");
    }

    // ポストエフェクト
    // オフスクリーン描画結果をデバッグ表示する
    if (context_.offscreenRenderer) {
        context_.offscreenRenderer->DrawDebugGameViewImGui();
        context_.offscreenRenderer->DrawImGui();
    }

#endif
}
