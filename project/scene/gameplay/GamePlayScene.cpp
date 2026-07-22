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

    // 隨倥・邵ｺ蜷ｶ竏狗ｸｺ・ｦ context_ 驍ｨ讙守ｽｰ邵ｺ・ｫ陞溽判蟲ｩ邵ｲ・ｴceneManager_ 邵ｺ・ｯ騾ｶ・ｴ隰暦ｽ･闖ｴ・ｿ騾包ｽｨ邵ｲ繝ｻ
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

    // 3D 郢ｧ・ｪ郢晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢昜ｺ･繝ｻ鬨ｾ繝ｻ
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(context_.object3dCommon);

    // 郢晢ｽ｢郢昴・ﾎ晞坡・ｭ邵ｺ・ｿ髴趣ｽｼ邵ｺ・ｿ
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("cube.obj");
    ModelManager::GetInstance()->LoadModel("multiMesh.obj");
    ModelManager::GetInstance()->LoadModel("multiMaterial.obj");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");
    ModelManager::GetInstance()->LoadModel("weapon/sword.obj");

    // 郢昴・縺醍ｹｧ・ｹ郢昶・ﾎ・
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle2.png");
    texMan->LoadTexture("Resources/fence.png");
    texMan->LoadTexture("Resources/Cube.png");
    texMan->LoadTexture("Resources/skybox.dds");
    texMan->LoadTexture("Resources/gradationLine.png"); // // Ring 騾包ｽｨ邵ｺ・ｮ郢ｧ・ｰ郢晢ｽｩ郢昴・繝ｻ郢ｧ・ｷ郢晢ｽｧ郢晢ｽｳ郢昴・縺醍ｹｧ・ｹ郢昶・ﾎ・

    // MultiMesh邵ｺ・ｨMultiMaterial邵ｺ・ｮ髫ｧ遨ゑｽｾ・｡驕抵ｽｺ髫ｱ蜥ｲ逡醍ｹ晢ｽ｢郢昴・ﾎ晉ｹｧ蜑・ｽｸ・ｦ邵ｺ・ｹ邵ｺ・ｦ髯ｦ・ｨ驕会ｽｺ邵ｺ蜷ｶ・・
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
    // 隰・ｹ敖ｰ郢ｧ蟲ｨ繝ｱ郢晢ｽｼ郢昴・縺・ｹｧ・ｯ郢晢ｽｫ郢ｧ雋槭・邵ｺ蜷ｶ笳・ｹｧ竏壹・郢ｧ・ｹ郢ｧ・ｭ郢昜ｹ斟ｦ郢ｧ・ｰ雋ょ現竏ｩhuman郢晢ｽ｢郢昴・ﾎ晉ｹｧ蝣､逡題ｫ｢荳岩・郢ｧ繝ｻ
    humanObject_ = std::make_unique<Object3d>();
    humanObject_->Initialize(context_.object3dCommon);
    humanObject_->SetModel("human/walk.gltf");
    humanObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    humanObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    humanAnimation_ = LoadAnimationFile("Resources/human", "walk.gltf");
    humanSneakWalkAnimation_ = LoadAnimationFile("Resources/human", "sneakWalk.gltf");
    humanObject_->SetAnimation(humanAnimation_);
    humanObject_->SetIsAnimationPlaying(true);

    // 陷ｿ・ｳ隰・ｹ昶・隰問・笳・ｸｺ蟶呻ｽ狗ｹ昴・繝ｰ郢昴・縺帝包ｽｨ邵ｺ・ｮ陷托ｽ｣郢晢ｽ｢郢昴・ﾎ晉ｹｧ蜑・ｽｽ諛奇ｽ・
    weaponObject_ = std::make_unique<Object3d>();
    weaponObject_->Initialize(context_.object3dCommon);
    weaponObject_->SetModel("weapon/sword.obj");
    weaponObject_->SetScale(weaponScale_);
    weaponObject_->SetRotate(weaponRotate_);
    weaponObject_->SetTranslate(rightHandPosition_);

    // 隰・ｹ昴・Joint闖ｴ蜥ｲ・ｽ・ｮ邵ｺ荵晢ｽ芽ｬ・唱陌哘mit邵ｺ蜷ｶ・狗ｹ昜ｻ｣繝ｻ郢昴・縺・ｹｧ・ｯ郢晢ｽｫ郢ｧ・ｷ郢ｧ・ｹ郢昴・ﾎ堤ｹｧ蜑・ｽｽ諛奇ｽ・
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

    // 郢昴・繝ｰ郢昴・縺帝包ｽｻ鬮ｱ・｢邵ｺ・ｧ郢ｧ・ｫ郢晢ｽ｡郢晢ｽｩ郢ｧ蜻域｡・抄諛岩・郢ｧ荵昴￠郢晢ｽｩ郢ｧ・ｹ郢ｧ蝣､蜃ｽ隰瑚・笘・ｹｧ繝ｻ
    debugCamera_ = std::make_unique<DebugCamera>();

}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

    // Skybox郢ｧ蜻茨ｽｯ蠑ｱ繝ｵ郢晢ｽｬ郢晢ｽｼ郢晢｣ｰ隴厄ｽｴ隴・ｽｰ邵ｺ蜷ｶ・・
    if (skybox_) { skybox_->Update(); }

    // 郢昴・繝ｰ郢昴・縺堤ｹ晢ｽ｢郢晢ｽｼ郢晄・・ｸ・ｭ邵ｺ・ｯDebugCamera邵ｺ・ｧ郢ｧ・ｫ郢晢ｽ｡郢晢ｽｩ郢ｧ蜻域｡・抄諛岩・郢ｧ繝ｻ
    if (debugCamera_ && context_.isDebugMode) {
        debugCamera_->Update(
            context_.camera,
            context_.input,
            context_.offscreenRenderer,
            *context_.isDebugMode);
    }

    if (context_.camera) { context_.camera->Update(); }

    // human郢ｧ雋槭・邵ｺ・ｫ隴厄ｽｴ隴・ｽｰ邵ｺ蜉ｱ窶ｻ邵ｲ竏ｵ諤呵ｭ・ｽｰ邵ｺ・ｮ隰・ｹ昴・Joint闖ｴ蜥ｲ・ｽ・ｮ郢ｧ蜑・ｽｽ・ｿ邵ｺ蛹ｻ・狗ｹｧ蛹ｻ竕ｧ邵ｺ・ｫ邵ｺ蜷ｶ・・
    if (humanObject_) {
        if (humanAnimationBlending_) {
            UpdateHumanAnimationBlend(dt);
        }
        humanObject_->Update();
    }

    // human隴厄ｽｴ隴・ｽｰ陟募ｾ後・隴崢隴・ｽｰJoint闖ｴ蜥ｲ・ｽ・ｮ郢ｧ蛛ｵﾂ竏壹Τ郢晢ｽｼ郢昴・縺・ｹｧ・ｯ郢晢ｽｫ邵ｺ・ｨ雎・ｽｦ陜趣ｽｨ邵ｺ・ｮ闕ｳ・｡隴・ｽｹ邵ｺ・ｧ闖ｴ・ｿ邵ｺ繝ｻ
    UpdateHandJointPositions();
    UpdateWeaponTransform();

    // MultiMesh/MultiMaterial驕抵ｽｺ髫ｱ蜥ｲ逡醍ｹ晢ｽ｢郢昴・ﾎ晉ｸｺ・ｮ髯ｦ謔溘・郢ｧ蜻亥ｳｩ隴・ｽｰ邵ｺ蜷ｶ・・
    if (multiMeshObject_) { multiMeshObject_->Update(); }
    if (multiMaterialObject_) { multiMaterialObject_->Update(); }

    // 隰・ｹ昴・Joint闖ｴ蜥ｲ・ｽ・ｮ邵ｺ荵晢ｽ臥ｹ昜ｻ｣繝ｻ郢昴・縺・ｹｧ・ｯ郢晢ｽｫ郢ｧ蝣､蛹ｱ騾墓ｺ假ｼ・ｸｺ蟶呻ｽ・
    EmitHandParticles(dt);

    // 騾具ｽｺ騾墓ｻ難ｽｸ蛹ｻ竏ｩ郢昜ｻ｣繝ｻ郢昴・縺・ｹｧ・ｯ郢晢ｽｫ郢ｧ蜻亥ｳｩ隴・ｽｰ邵ｺ蜷ｶ・・
    if (particleSystem_) { particleSystem_->Update(dt); }
}

void GamePlayScene::Draw()
{
    // 隨倥・context_ 驍ｨ讙守ｽｰ邵ｺ・ｫ陞溽判蟲ｩ
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


    // MultiMesh/MultiMaterial驕抵ｽｺ髫ｱ蜥ｲ逡醍ｹ晢ｽ｢郢昴・ﾎ晉ｹｧ蜻育ｷ帝包ｽｻ邵ｺ蜷ｶ・・
    context_.object3dCommon->CommonDrawSetting();
    if (showWeaponObject_ && weaponObject_) { weaponObject_->Draw(); }
    if (showHumanObject_ && humanObject_) { humanObject_->Draw(); }
    if (showMultiMeshMaterialDemo_) {
        if (multiMeshObject_) { multiMeshObject_->Draw(); }
        if (multiMaterialObject_) { multiMaterialObject_->Draw(); }
    }

    // Particle
    context_.particleCommon->CommonDrawSetting();

    // Particle郢ｧ蜻育ｷ帝包ｽｻ邵ｺ蜷ｶ・・
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

void GamePlayScene::ApplyHumanAnimationSelection()
{
    if (!humanObject_) {
        return;
    }

    // Human ImGui邵ｺ・ｧ鬩包ｽｸ郢ｧ阮吮味郢ｧ・｢郢昜ｹ斟鍋ｹ晢ｽｼ郢ｧ・ｷ郢晢ｽｧ郢晢ｽｳ郢ｧ讙uman郢晢ｽ｢郢昴・ﾎ晉ｸｺ・ｸ陷ｿ閧ｴ荳千ｸｺ蜷ｶ・・
    if (humanAnimationIndex_ == 1) {
        humanObject_->SetAnimation(humanSneakWalkAnimation_);
    } else {
        humanObject_->SetAnimation(humanAnimation_);
    }

    humanPreviewTime_ = 0.0f;
    humanObject_->ResetAnimationTime();
    humanObject_->SetIsAnimationPlaying(!humanTpose_);
    if (humanTpose_) {
        humanObject_->ResetSkeletonPose();
    }
}

const Animation& GamePlayScene::GetHumanAnimationByIndex(int animationIndex) const
{
    if (animationIndex == 1) {
        return humanSneakWalkAnimation_;
    }
    return humanAnimation_;
}

void GamePlayScene::StartHumanAnimationBlend(int nextAnimationIndex)
{
    if (!humanObject_) {
        return;
    }
    if (nextAnimationIndex < 0 || nextAnimationIndex > 1) {
        return;
    }
    if (!humanAnimationBlending_ && nextAnimationIndex == humanAnimationIndex_) {
        return;
    }

    const Animation& fromAnimation = GetHumanAnimationByIndex(humanAnimationIndex_);
    const Animation& toAnimation = GetHumanAnimationByIndex(nextAnimationIndex);

    humanBlendFromAnimation_ = fromAnimation;
    humanBlendToAnimation_ = toAnimation;
    humanBlendFromTime_ = humanObject_->GetAnimationTime();

    float phase = 0.0f;
    if (fromAnimation.duration > 0.0f) {
        phase = humanBlendFromTime_ / fromAnimation.duration;
    }
    humanBlendToTime_ = toAnimation.duration * phase;
    humanBlendTimer_ = 0.0f;
    humanAnimationIndex_ = nextAnimationIndex;
    humanTpose_ = false;
    humanObject_->SetIsAnimationPlaying(false);

    if (humanBlendDuration_ <= 0.0f) {
        ApplyHumanAnimationSelection();
        return;
    }

    // Combo変更時に即差し替えず、一定時間かけてWalk/Sneakの姿勢を混ぜる
    humanAnimationBlending_ = true;
}

void GamePlayScene::UpdateHumanAnimationBlend(float deltaTime)
{
    if (!humanObject_ || !humanAnimationBlending_) {
        return;
    }
    if (humanBlendDuration_ <= 0.0f) {
        humanAnimationBlending_ = false;
        ApplyHumanAnimationSelection();
        return;
    }

    humanBlendTimer_ += deltaTime;
    humanBlendFromTime_ += deltaTime;
    humanBlendToTime_ += deltaTime;

    float blendRate = humanBlendTimer_ / humanBlendDuration_;
    if (blendRate >= 1.0f) {
        humanAnimationBlending_ = false;
        humanObject_->SetAnimation(GetHumanAnimationByIndex(humanAnimationIndex_));
        humanObject_->ApplyAnimationPose(humanBlendToTime_);
        humanObject_->SetIsAnimationPlaying(!humanTpose_);
        return;
    }

    // 毎フレーム、元アニメと先アニメの同じ進行付近の姿勢をブレンドして表示する
    humanObject_->SetIsAnimationPlaying(false);
    humanObject_->ApplyAnimationBlendPose(
        humanBlendFromAnimation_,
        humanBlendFromTime_,
        humanBlendToAnimation_,
        humanBlendToTime_,
        blendRate);
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

    // Joint邵ｺ・ｮSkeleton驕ｨ・ｺ鬮｢讌｢・｡謔溘・邵ｺ・ｫ郢晢ｽ｢郢昴・ﾎ晉ｸｺ・ｮWorld髯ｦ謔溘・郢ｧ蜻亥ｯｺ邵ｺ莉｣窶ｻ邵ｲ竏ｵ辟皮ｸｺ・ｮ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥・ｺ・ｧ隶灘生・定愾謔ｶ・願怎・ｺ邵ｺ繝ｻ
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

    // Joint邵ｺ・ｮSkeleton驕ｨ・ｺ鬮｢讌｢・｡謔溘・邵ｺ・ｫhuman邵ｺ・ｮWorld髯ｦ謔溘・郢ｧ蜻亥ｯｺ邵ｺ莉｣窶ｻ邵ｲ繧孃int邵ｺ・ｮWorld髯ｦ謔溘・郢ｧ蜑・ｽｽ諛奇ｽ・
    const Joint& joint = skeleton.joints[jointIt->second];
    worldMatrix = Multiply(joint.skeletonSpaceMatrix, humanWorldMatrix);

    // Joint髯ｦ謔溘・邵ｺ・ｫ郢ｧ・ｹ郢ｧ・ｱ郢晢ｽｼ郢晢ｽｫ邵ｺ譴ｧ・ｷ・ｷ邵ｺ蛟･・狗ｸｺ・ｨ雎・ｽｦ陜趣ｽｨ邵ｺ譴ｧ・ｶ蛹ｻ竏ｴ郢ｧ荵昴・邵ｺ・ｧ邵ｲ竏晏ｱ馴怕・｢髴・ｽｸ邵ｺ・ｰ邵ｺ隨ｬ・ｭ・｣髫穂ｸ槫密邵ｺ蜷ｶ・・
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
    // 闕ｳ・ｭ隰悶・・ｻ蛟･・隴ｬ・ｹ邵ｺ・ｮ陷ｷ莉｣窶ｳ郢ｧ蜑・ｽｽ・ｿ邵ｺ繝ｻﾂ竏ｽ・ｽ蜥ｲ・ｽ・ｮ邵ｺ・ｰ邵ｺ隨ｬ辟秘ｬ･謔ｶ繝ｻ隰悶・繝ｻ髫包ｽｪ隰悶・繝ｻ闕ｳ・ｭ鬮｢阮吮・陝・・笳狗ｸｺ貊会ｽｻ・ｮ隲・ｳWeaponSocket郢ｧ蜑・ｽｽ諛奇ｽ・
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
        // 隰悶・繝ｻ闔牙･・隴ｬ・ｹ邵ｺ・ｨ髫包ｽｪ隰悶・繝ｻ鬮｢阮吮・驗ゑｽｮ邵ｺ荳岩・邵ｲ竏ｵ辟秘ｬ･謔ｶ・育ｹｧ鄙ｫﾂ譴ｧ蜊・ｸｺ・｣邵ｺ・ｦ邵ｺ繝ｻ・狗ｸｲ蝣ｺ・ｽ蜥ｲ・ｽ・ｮ邵ｺ・ｫ髫穂ｹ昶斡郢ｧ繝ｻ笘・ｸｺ繝ｻ
        worldMatrix.m[3][0] = handPosition.x * 0.25f + indexPosition.x * 0.20f + middlePosition.x * 0.25f + ringPosition.x * 0.15f + thumbPosition.x * 0.15f;
        worldMatrix.m[3][1] = handPosition.y * 0.25f + indexPosition.y * 0.20f + middlePosition.y * 0.25f + ringPosition.y * 0.15f + thumbPosition.y * 0.15f;
        worldMatrix.m[3][2] = handPosition.z * 0.25f + indexPosition.z * 0.20f + middlePosition.z * 0.25f + ringPosition.z * 0.15f + thumbPosition.z * 0.15f;
    }

    return true;
}
void GamePlayScene::UpdateHandJointPositions()
{
    // Mixamo陷ｷ髦ｪ竊帝￥・ｭ邵ｺ繝ｻ骭占恆髦ｪ繝ｻ闕ｳ・｡隴・ｽｹ郢ｧ蜻育粟邵ｺ蜉ｱ窶ｻ邵ｲ竏墅皮ｹ昴・ﾎ晁淦・ｮ邵ｺ邇ｲ蟠帷ｸｺ蝓溷・邵ｺ・ｫ郢ｧ繧育・郢ｧ蜻磯升邵ｺ蛹ｻ・狗ｹｧ蛹ｻ竕ｧ邵ｺ・ｫ邵ｺ蜷ｶ・・
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
        // WeaponSocket騾ｶ・ｸ陟冶侭繝ｻ郢晢ｽｭ郢晢ｽｼ郢ｧ・ｫ郢晢ｽｫ髯ｬ諛茨ｽｭ・｣郢ｧ蜑・ｽｽ諛奇ｽ顔ｸｲ竏晄価隰・ｾｽoint邵ｺ・ｮWorld髯ｦ謔溘・邵ｺ・ｸ髫包ｽｪ陝・揄・ｻ蛟･・邵ｺ蜷ｶ・・
        Matrix4x4 weaponLocalMatrix = MakeAffineMatrix(weaponScale_, weaponRotate_, weaponOffset_);
        Matrix4x4 weaponWorldMatrix = reverseWeaponMatrixOrder_
            ? Multiply(rightHandWorldMatrix_, weaponLocalMatrix)
            : Multiply(weaponLocalMatrix, rightHandWorldMatrix_);
        weaponObject_->SetWorldMatrix(weaponWorldMatrix);
    } else {
        // 髯ｦ謔溘・隰暦ｽ･驍ｯ螢ｹ・定崕繝ｻ笆ｲ邵ｺ貅倪・邵ｺ髦ｪ繝ｻ邵ｲ竏晄価隰・ｶ・ｽ蜥ｲ・ｽ・ｮ + offset 邵ｺ・ｮ驕抵ｽｺ髫ｱ蜥ｲ逡鷹勗・ｨ驕会ｽｺ邵ｺ・ｫ隰鯉ｽｻ邵ｺ繝ｻ
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


    // 隰・ｹ敖ｰ郢ｧ迚吶・邵ｺ・ｦ邵ｺ繝ｻ・狗ｸｺ阮吮・邵ｺ迹夲ｽｦ荵昶斡郢ｧ繝ｻ笘・ｸｺ繝ｻ・育ｸｺ繝ｻ竊鍋ｸｲ竏晢ｽｰ莉｣・闕ｳ髮・ｫ・ｸｺ髦ｪ縲帝￥・ｭ陷ｻ・ｽ邵ｺ・ｮ驍雁・・定怎・ｺ邵ｺ繝ｻ
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

    // 郢ｧ・ｷ郢晢ｽｼ郢晢ｽｳ驕抵ｽｺ髫ｱ蜥ｲ逡醍ｸｺ・ｮ郢ｧ・ｫ郢晢ｽ｡郢晢ｽｩ髫ｪ・ｭ陞ｳ螢ｹ・堤ｸｺ・ｾ邵ｺ・ｨ郢ｧ竏壺ｻ髯ｦ・ｨ驕会ｽｺ邵ｺ蜷ｶ・・
    ImGui::Begin("Scene Camera");
    ImGui::DragFloat3("Translate", &cameraTransform.translate.x, 0.1f);
    ImGui::DragFloat3("Rotate", &cameraTransform.rotate.x, 0.01f);
    ImGui::Text("Skybox and objects use this camera.");
    ImGui::End();

    // human郢晢ｽ｢郢昴・ﾎ晉ｸｺ・ｮ髯ｦ・ｨ驕会ｽｺ邵ｲ竏昴・騾墓ｺ伉繧晉ｹ晄亢繝ｻ郢ｧ・ｺ邵ｲ竏ｬ・｣諞ｺ菫｣驕抵ｽｺ髫ｱ髦ｪ・堤ｸｺ・ｾ邵ｺ・ｨ郢ｧ竏壺ｻ隰ｫ蝣ｺ・ｽ諛岩・郢ｧ繝ｻ
    ImGui::Begin("Human");
    ImGui::Checkbox("Show Human", &showHumanObject_);
    if (humanObject_) {
        bool isPlaying = humanObject_->IsAnimationPlaying();
        if (ImGui::Checkbox("Play Animation", &isPlaying)) {
            humanTpose_ = false;
            humanAnimationBlending_ = false;
            humanObject_->SetIsAnimationPlaying(isPlaying);
        }

        // 読み込んだ複数アニメーションをブレンド付きで切り替える
        const char* animationItems[] = { "Walk", "Sneak Walk" };
        int requestedAnimationIndex = humanAnimationIndex_;
        if (ImGui::Combo("Animation", &requestedAnimationIndex, animationItems, 2)) {
            StartHumanAnimationBlend(requestedAnimationIndex);
        }
        ImGui::SliderFloat("Blend Duration", &humanBlendDuration_, 0.0f, 1.5f, "%.2f");
        if (humanAnimationBlending_ && humanBlendDuration_ > 0.0f) {
            ImGui::Text("Blend %.2f", humanBlendTimer_ / humanBlendDuration_);
        }

        if (ImGui::Button("Reset Time")) {
            humanAnimationBlending_ = false;
            humanPreviewTime_ = 0.0f;
            humanObject_->ResetAnimationTime();
            humanObject_->ApplyAnimationPose(0.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("T Pose")) {
            humanAnimationBlending_ = false;
            humanTpose_ = true;
            humanObject_->SetIsAnimationPlaying(false);
            humanObject_->ResetSkeletonPose();
        }

        float duration = humanObject_->GetAnimationDuration();
        humanPreviewTime_ = humanObject_->GetAnimationTime();
        // スライダーで任意時刻の姿勢を反映し、アニメーション内の補間結果を確認する
        if (duration > 0.0f) {
            if (ImGui::SliderFloat("Pose Preview Time", &humanPreviewTime_, 0.0f, duration, "%.2f")) {
                humanTpose_ = false;
                humanAnimationBlending_ = false;
                humanObject_->SetIsAnimationPlaying(false);
                humanObject_->ApplyAnimationPose(humanPreviewTime_);
            }
        }

        ImGui::Text("Time %.2f / %.2f", humanObject_->GetAnimationTime(), duration);
        ImGui::Text("Animated Nodes %zu", humanObject_->GetAnimationNodeCount());
        ImGui::Text("Left  %.2f %.2f %.2f", leftHandPosition_.x, leftHandPosition_.y, leftHandPosition_.z);
        ImGui::Text("Grip  %.2f %.2f %.2f", rightHandPosition_.x, rightHandPosition_.y, rightHandPosition_.z);
    }
    ImGui::End();

    // 陷托ｽ｣邵ｺ・ｮ髯ｦ・ｨ驕会ｽｺ邵ｺ・ｨ隰・ｹ昶・邵ｺ・ｮ髴托ｽｽ陟墓･｢・ｪ・ｿ隰ｨ・ｴ郢ｧ蛛ｵ竏ｪ邵ｺ・ｨ郢ｧ竏壺ｻ隰ｫ蝣ｺ・ｽ諛岩・郢ｧ繝ｻ
    ImGui::Begin("Weapon");
    ImGui::Checkbox("Show Weapon", &showWeaponObject_);
    ImGui::Checkbox("Attach To Grip Socket", &attachWeaponToHand_);
    ImGui::Checkbox("Reverse Matrix Order", &reverseWeaponMatrixOrder_);
    ImGui::DragFloat3("Offset", &weaponOffset_.x, 0.01f);
    ImGui::DragFloat3("Rotate", &weaponRotate_.x, 0.01f);
    ImGui::DragFloat3("Scale", &weaponScale_.x, 0.01f, 0.01f, 2.0f);
    ImGui::End();

    // 隰・ｹ敖ｰ郢ｧ迚吶・邵ｺ蜷ｶ繝ｱ郢晢ｽｼ郢昴・縺・ｹｧ・ｯ郢晢ｽｫ邵ｺ・ｮ騾具ｽｺ騾墓ｻ捺套闔会ｽｶ郢ｧ蛛ｵ竏ｪ邵ｺ・ｨ郢ｧ竏壺ｻ隰ｫ蝣ｺ・ｽ諛岩・郢ｧ繝ｻ
    ImGui::Begin("Hand Particle");
    ImGui::Checkbox("Emit From Hands", &emitHandParticles_);
    ImGui::Checkbox("Left Hand", &emitLeftHandParticles_);
    ImGui::Checkbox("Right Hand", &emitRightHandParticles_);
    ImGui::SliderFloat("Emit Interval", &handParticleInterval_, 0.01f, 0.3f, "%.2f");
    ImGui::End();

    // MultiMesh邵ｺ・ｨMultiMaterial驕抵ｽｺ髫ｱ蜥ｲ逡醍ｹ晢ｽ｢郢昴・ﾎ晉ｹｧ蛛ｵ竏ｪ邵ｺ・ｨ郢ｧ竏壺ｻ髯ｦ・ｨ驕会ｽｺ邵ｺ蜷ｶ・・
    ImGui::Begin("Demo Models");
    ImGui::Checkbox("Show Multi Mesh / Material", &showMultiMeshMaterialDemo_);
    ImGui::Text("multiMesh.obj : Plane + Cube");
    ImGui::Text("multiMaterial.obj : uvChecker + monsterBall");
    ImGui::End();

    if (particleSystem_) {
        particleSystem_->ShowImGui("Particle Detail");
    }

    if (context_.offscreenRenderer) {
        context_.offscreenRenderer->DrawDebugGameViewImGui();
        context_.offscreenRenderer->DrawImGui();
    }

#endif
}
