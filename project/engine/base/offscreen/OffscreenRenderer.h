#pragma once
#include "../DirectX/DirectXCommon.h"
#include "../srv/SrvManager.h"
#include "../renderTexture/RenderTexture.h"

class OffscreenRenderer {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    void PreDrawScene();
    void DrawToBackBuffer();

    RenderTexture* GetRenderTexture() const { return renderTexture_.get(); }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<RenderTexture> renderTexture_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
