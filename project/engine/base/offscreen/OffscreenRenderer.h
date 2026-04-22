#pragma once
#include "../DirectX/DirectXCommon.h"
#include "../srv/SrvManager.h"
#include "../renderTexture/RenderTexture.h"

enum class PostEffectType {
    Copy,
    Grayscale,
    Sepia,
    Vignette,
    BoxFilter,
    GaussianFilter,
};

class OffscreenRenderer {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    void PreDrawScene();
    void DrawToBackBuffer();

    RenderTexture* GetRenderTexture() const { return renderTexture_.get(); }
    // セッター
    void SetPostEffectType(PostEffectType type) { postEffectType_ = type; }
    PostEffectType GetPostEffectType() const { return postEffectType_; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<RenderTexture> renderTexture_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> copyPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> grayscalePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> sepiaPipelineState_;
    // ヴィネッティング用のパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_;
    // ボックスフィルタ用のパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilterPipelineState_;
    // ガウシアンフィルタ用のパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianFilterPipelineState_;

    PostEffectType postEffectType_ = PostEffectType::Copy;

};
