#pragma once

// 標準
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

// DirectX / COM
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// ImGui
#include "../externals/imgui/imgui.h"
#include "../externals/imgui/imgui_impl_win32.h"
#include "../externals/imgui/imgui_impl_dx12.h"

// 自作
#include "../engine/math/Math.h"
#include "../engine/input/Input.h"
#include "../engine/base/srv/SrvManager.h"
#include "../engine/base/DirectX/DirectXCommon.h"
#include "../engine/2d/sprite/SpriteCommon.h"
#include "../engine/2d/sprite/Sprite.h"
#include "../engine/2d/texture/TextureManager.h"
#include "../engine/3d/obj3d/Object3dCommon.h"
#include "../engine/3d/obj3d/Object3d.h"
#include "../engine/3d/model/ModelCommon.h"
#include "../engine/3d/model/ModelManager.h"
#include "../engine/particle/ParticleCommon.h"
#include "../engine/particle/Particle.h"
#include "../game/camera/Camera.h"
#include "../engine/base/srv/SrvManager.h"
#include "../engine/base/imgui/ImGuiManager.h"
#include "../app/Framework.h"
#include "../scene/BaseScene.h"    // ★追加
#include "../scene/SceneManager.h"
#include "../engine/audio/SoundManager.h"
#include "../engine/base/winapp/WinApp.h"
#include "../engine/base/offscreen/OffscreenRenderer.h"

// ライブラリリンク（ここにまとめておく）
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "xinput.lib")

class GamePlayScene; // ★追加（インクルード名は増やさない）
class TitleScene;

class Game : public Framework
{
public:
    Game() = default;
    ~Game() = default;

    void Initialize()override;
    void Update()override;
    void Draw()override;
    void Finalize()override;

private:

    std::unique_ptr<WinApp> winApp_;
    std::unique_ptr<Input> input_;
    std::unique_ptr<SoundManager> sound_;
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<SrvManager> srvManager_;
    std::unique_ptr<SpriteCommon> spriteCommon_;
    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<ModelCommon> modelCommon_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<ParticleCommon> particleCommon_;
    std::unique_ptr<ImGuiManager> imguiManager_;
    std::unique_ptr<SceneManager> sceneManager_;
    std::unique_ptr<OffscreenRenderer> offscreenRenderer_;

#ifdef USE_IMGUI
    // デバッグUIを表示するかどうか
    bool isDebugMode_ = false;
#endif

};
