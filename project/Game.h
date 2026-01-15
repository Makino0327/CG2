#pragma once

// ===============================
// 標準
// ===============================
#include <Windows.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <wrl.h>

// ===============================
// DirectX
// ===============================
#include <d3d12.h>
#include <dxgi1_6.h>

// ===============================
// ImGui
// ===============================
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"

// ===============================
// 自作エンジン
// ===============================
#include "Math.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "D3DResourceLeakChecker.h"

#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"

#include "Object3dCommon.h"
#include "Object3d.h"

#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"

#include "ParticleCommon.h"
#include "Particle.h"

#include "Camera.h"
#include "SrvManager.h"
#include "ImGuiManager.h"

// ===============================
// Game
// ===============================
class Game {
public:
    Game() = default;
    ~Game() = default;

    // 初期化のみ
    bool Initialize();

    // main から使うための getter（形を壊さない用）
    WinApp* GetWinApp() const { return winApp_; }
    Input* GetInput() const { return input_; }
    DirectXCommon* GetDXCommon() const { return dxCommon_; }

private:
    // -----------------------------
    // システム
    // -----------------------------
    WinApp* winApp_ = nullptr;
    Input* input_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // -----------------------------
    // Sprite
    // -----------------------------
    SpriteCommon* spriteCommon_ = nullptr;
    std::vector<Sprite*> sprites_;

    // -----------------------------
    // 3D
    // -----------------------------
    Object3dCommon* object3dCommon_ = nullptr;
    Object3d* object3d_ = nullptr;

    // -----------------------------
    // Model
    // -----------------------------
    ModelCommon* modelCommon_ = nullptr;

    // -----------------------------
    // Camera
    // -----------------------------
    Camera* camera_ = nullptr;

    // -----------------------------
    // Particle
    // -----------------------------
    static const uint32_t kNumInstance_ = 10;
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    ParticleCommon* particleCommon_ = nullptr;
    ParticleSystem* particleSystem_ = nullptr;

    // -----------------------------
    // ImGui
    // -----------------------------
    ImGuiManager* imguiManager_ = nullptr;

    float deltaTime_ = 1.0f / 60.0f;
};
