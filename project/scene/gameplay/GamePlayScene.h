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
    std::unique_ptr<Object3d> objA_;

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
};
