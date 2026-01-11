#pragma once

#include "WinApp.h" // WinAppの型を認識させるために追加
#include "DirectXCommon.h"
#include "srvManager.h"

class ImGuiManager
{
public:
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager, ID3D12DescriptorHeap* srvHeap);

	void Finalize();

	void Begin();
	void End();
	void Draw();

private:
	uint32_t imguiSrvIndex_ = 0;
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

};
