#pragma once

#include <vector>
#include <string>
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "../engine/math/Math.h"
#include "../BaseScene.h"
#include "../engine/audio/SoundManager.h"
#include "../engine/3d/skybox/Skybox.h"
#include "../engine/3d/skybox/SkyboxCommon.h"
#include "../engine/animation/Animation.h"
#include "../../engine/3d/obj3d/Line3DCommon.h"
#include "../../engine/3d/obj3d/DebugSkeletonRenderer.h"
#include "../../game/camera/DebugCamera.h"


// ===== 前方宣言 =====
// SceneContext.h を BaseScene.h がインクルードしている前提なので、
// ここで個別のコンポーネントを大量に宣言する必要はなくなります
class Sprite;
class Object3d;
class ParticleSystem;
class Skybox;
class SkyboxCommon;

//==================================================
// GamePlayScene
//==================================================
class GamePlayScene : public BaseScene
{
public:
    // ===== BaseScene interface（SceneManagerから呼ばれる）=====
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void DrawImGui() override;

    // ★ SetContext は BaseScene で共通化されたため、ここからは削除！

private:

    // ===== シーン固有（GamePlaySceneが所有）=====
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<Object3d> humanObject_;
    std::unique_ptr<Object3d> weaponObject_;
    std::unique_ptr<Object3d> multiMeshObject_;
    std::unique_ptr<Object3d> multiMaterialObject_;
    Animation humanAnimation_;
    bool showMultiMeshMaterialDemo_ = true;
    bool showHumanObject_ = true;
    bool emitHandParticles_ = true;
    bool emitLeftHandParticles_ = true;
    bool emitRightHandParticles_ = true;
    bool showWeaponObject_ = true;
    float handParticleInterval_ = 0.05f;
    float handParticleTimer_ = 0.0f;
    Vector3 leftHandPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rightHandPosition_ = { 0.0f, 0.0f, 0.0f };
    Matrix4x4 rightHandWorldMatrix_{};
    Vector3 weaponOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 weaponRotate_ = { 0.0f, 0.0f, 1.570796f };
    Vector3 weaponScale_ = { 0.18f, 0.18f, 0.18f };
    bool leftHandFound_ = false;
    bool rightHandFound_ = false;
    bool attachWeaponToHand_ = true;
    bool reverseWeaponMatrixOrder_ = false;

    bool GetJointWorldPosition(const std::string& jointName, Vector3& worldPosition) const;
    bool GetJointWorldMatrix(const std::string& jointName, Matrix4x4& worldMatrix) const;
    bool GetGripSocketWorldMatrix(Matrix4x4& worldMatrix) const;
    void UpdateHandJointPositions();
    void UpdateWeaponTransform();
    void EmitHandParticles(float deltaTime);

    std::unique_ptr<SkyboxCommon> skyboxCommon_;
    std::unique_ptr<Skybox> skybox_;

    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    bool initialized_ = false; // 二重初期化防止

    std::unique_ptr<DebugCamera> debugCamera_;

};
