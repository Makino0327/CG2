#pragma once

// 標準
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

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

// ライブラリリンク（ここにまとめておく）
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "xinput.lib")

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

	// Windows / Input / DX
	WinApp* winApp_ = nullptr;
	Input* input_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	// SRV / Texture
	SrvManager* srvManager_ = nullptr;

	// Sprite
	SpriteCommon* spriteCommon_ = nullptr;
	std::vector<Sprite*> sprites_;
	Vector2 spritePos_{ 100.0f, 100.0f };

	// 3D
	Object3dCommon* object3dCommon_ = nullptr;
	Object3d* object3d_ = nullptr;      // あなたの元コードにあったので保持（今は未使用でもOK）
	Object3d* objA_ = nullptr;          // ループ内で使ってるやつ

	// Model
	ModelCommon* modelCommon_ = nullptr;

	// Camera
	Camera* camera_ = nullptr;

	// Particle
	ParticleCommon* particleCommon_ = nullptr;
	ParticleSystem* particleSystem_ = nullptr;
	float deltaTime_ = 1.0f / 60.0f;

	// 定数バッファ（元コードのまま保持）
	ComPtr<ID3D12Resource> materialResource_;
	ComPtr<ID3D12Resource> directionalLightResource_;

	// ImGui
	ImGuiManager* imguiManager_ = nullptr;
};
