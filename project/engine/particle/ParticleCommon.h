#pragma once
#include <d3d12.h>
#include "../base/DirectX/DirectXCommon.h"
#include "../base/srv/SrvManager.h"

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

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	
	SrvManager* srvManager_ = nullptr;

};

