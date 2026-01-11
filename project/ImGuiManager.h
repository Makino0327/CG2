#pragma once

#include "WinApp.h" // WinAppの型を認識させるために追加
#include "DirectXCommon.h"
#include "srvManager.h"

class ImGuiManager
{
public:
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);

private:
	uint32_t imguiSrvIndex_ = 0;
};
