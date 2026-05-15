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
    // ★ 個別のポインタ群（dxCommon_ など）もすべて削除！
    // 親クラス BaseScene の context_ を使います。

    // ===== シーン固有（GamePlaySceneが所有）=====
    std::unique_ptr<Object3d> object3d_;
    // simpleSkin 用
    std::unique_ptr<Object3d> objA_;

    // human 用
    std::unique_ptr<Object3d> objB_;


    std::unique_ptr<SkyboxCommon> skyboxCommon_;
    std::unique_ptr<Skybox> skybox_;

    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    Vector2 spritePos_ = { 0.0f, 0.0f };

    bool initialized_ = false; // 二重初期化防止

    SoundData seSelect_;
    bool soundLoaded_ = false;

    // Ring 用パーティクル
    std::unique_ptr<ParticleSystem> ringParticleSystem_;

    std::unique_ptr<ParticleSystem> cylinderParticleSystem_;

    // 線描画共通
    std::unique_ptr<Line3DCommon> line3dCommon_;

    // simpleSkin 用 Skeleton デバッグ描画
    std::unique_ptr<DebugSkeletonRenderer> debugSkeletonRendererA_;

    // human 用 Skeleton デバッグ描画
    std::unique_ptr<DebugSkeletonRenderer> debugSkeletonRendererB_;


    // simpleSkin 確認用 transform
    Vector3 objATranslate_ = { -1.0f, 0.0f, 0.0f };
    Vector3 objARotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 objAScale_ = { 1.0f, 1.0f, 1.0f };

    // human 確認用 transform
    Vector3 objBTranslate_ = { 1.0f, 0.0f, 0.0f };
    Vector3 objBRotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 objBScale_ = { 1.0f, 1.0f, 1.0f };

    // human animation の再生確認用
    bool isHumanAnimationPlaying_ = false;

    std::unique_ptr<DebugCamera> debugCamera_;

};
