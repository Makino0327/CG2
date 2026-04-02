#pragma once
#include "../obj3d/Object3dCommon.h"
#include "../../base/DirectX/DirectXCommon.h"
class ModelCommon
{
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

private:
	// 
	DirectXCommon* dxCommon_;

public:
	// ゲッター
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
};

