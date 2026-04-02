#pragma once
#include "../winapp/WinApp.h" // WinAppの型を認識させるために追加
#include "../DirectX/DirectXCommon.h"
#include "../srv/SrvManager.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "../externals/imgui/imgui_impl_win32.h"
#include "../externals/imgui/imgui_impl_dx12.h"
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
	ID3D12DescriptorHeap* srvHeap_ = nullptr;

};
