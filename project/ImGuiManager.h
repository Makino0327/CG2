#pragma once
#include "WinApp.h" // WinAppの型を認識させるために追加
#include "DirectXCommon.h"
#include "srvManager.h"
#ifdef USE_IMGUI
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"
#endif

class ImGuiManager
{
public:
	void Initialize(
		[[maybe_unused]] WinApp* winApp,
		[[maybe_unused]] DirectXCommon* dxCommon,
		[[maybe_unused]] SrvManager* srvManager,
		[[maybe_unused]] ID3D12DescriptorHeap* srvHeap);

	void Finalize();

	void Begin();
	void End();
	void Draw();

private:
	uint32_t imguiSrvIndex_ = 0;
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

};
