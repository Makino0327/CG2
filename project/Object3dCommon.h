#pragma once
#include <d3d12.h>
#include "DirectXCommon.h"

class Object3dCommon
{
public:
    void Initialize(DirectXCommon* dxCommon);
    void CommonDrawSetting();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

private:
    DirectXCommon* dxCommon_ = nullptr;

    ID3D12RootSignature* rootSignature_ = nullptr;
    ID3D12PipelineState* pipelineState_ = nullptr;
};
