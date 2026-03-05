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
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"

// 自作
#include "Math.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "ParticleCommon.h"
#include "Particle.h"
#include "Camera.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "Framework.h"
#include "BaseScene.h"    // ★追加
#include "SceneManager.h"
#include "SoundManager.h"

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
};
