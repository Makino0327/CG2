#pragma once
#include "../obj3d/Object3dCommon.h"
#include "../../base/DirectX/DirectXCommon.h"

class SrvManager;

class ModelCommon
{
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	SrvManager* GetSrvManager() const { return srvManager_; }


private:
	// 
	DirectXCommon* dxCommon_;
	SrvManager* srvManager_ = nullptr;

public:
	// ゲッター
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	
};

