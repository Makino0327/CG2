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


// ===== 蜑肴婿螳｣險 =====
// SceneContext.h 繧・BaseScene.h 縺後う繝ｳ繧ｯ繝ｫ繝ｼ繝峨＠縺ｦ縺・ｋ蜑肴署縺ｪ縺ｮ縺ｧ縲・
// 縺薙％縺ｧ蛟句挨縺ｮ繧ｳ繝ｳ繝昴・繝阪Φ繝医ｒ螟ｧ驥上↓螳｣險縺吶ｋ蠢・ｦ√・縺ｪ縺上↑繧翫∪縺・
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
    // ===== BaseScene interface・・ceneManager縺九ｉ蜻ｼ縺ｰ繧後ｋ・・====
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void DrawImGui() override;

    // 笘・SetContext 縺ｯ BaseScene 縺ｧ蜈ｱ騾壼喧縺輔ｌ縺溘◆繧√√％縺薙°繧峨・蜑企勁・・

private:

    // ===== 繧ｷ繝ｼ繝ｳ蝗ｺ譛会ｼ・amePlayScene縺梧園譛会ｼ・====
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<Object3d> humanObject_;
    std::unique_ptr<Object3d> weaponObject_;
    std::unique_ptr<Object3d> multiMeshObject_;
    std::unique_ptr<Object3d> multiMaterialObject_;
    Animation humanAnimation_;
    Animation humanSneakWalkAnimation_;
    Animation humanBlendFromAnimation_;
    Animation humanBlendToAnimation_;
    float humanBlendFromTime_ = 0.0f;
    float humanBlendToTime_ = 0.0f;
    float humanBlendTimer_ = 0.0f;
    float humanBlendDuration_ = 0.4f;
    bool humanAnimationBlending_ = false;
    int humanAnimationIndex_ = 0;
    float humanPreviewTime_ = 0.0f;
    bool humanTpose_ = false;
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
    void ApplyHumanAnimationSelection();
    const Animation& GetHumanAnimationByIndex(int animationIndex) const;
    void StartHumanAnimationBlend(int nextAnimationIndex);
    void UpdateHumanAnimationBlend(float deltaTime);
    void UpdateHandJointPositions();
    void UpdateWeaponTransform();
    void EmitHandParticles(float deltaTime);

    std::unique_ptr<SkyboxCommon> skyboxCommon_;
    std::unique_ptr<Skybox> skybox_;

    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    bool initialized_ = false; // 莠碁㍾蛻晄悄蛹夜亟豁｢

    std::unique_ptr<DebugCamera> debugCamera_;

};
