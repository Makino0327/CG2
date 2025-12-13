#pragma once
#include <d3d12.h>
#include "DirectXCommon.h"
#include "SrvManager.h"

class ParticleCommon
{
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void CommonDrawSetting();

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	void CreateRootSignature();
	void CreateGraphicsPipelineState();

private:
	DirectXCommon* dxCommon_ = nullptr;

	ID3D12RootSignature* rootSignature_ = nullptr;
	ID3D12PipelineState* pipelineState_ = nullptr;
	
	SrvManager* srvManager_ = nullptr;

};

