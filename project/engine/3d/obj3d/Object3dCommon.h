#pragma once
#include <d3d12.h>
#include "../../base/DirectX/DirectXCommon.h"
#include "../../../game/camera/Camera.h"

class Object3dCommon
{
public:
    void Initialize(DirectXCommon* dxCommon);
    void CommonDrawSetting();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
    Camera* GetDefaultCamera() const { return defaultCamera_; }


private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Camera* defaultCamera_ = nullptr;
};
