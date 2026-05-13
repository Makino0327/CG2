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
    RadialBlur,
    DepthOutline,
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
    struct RadialBlurData {
        Vector2 center;   // ブラーの中心UV
        float blurWidth;  // ブラーの強さ
        float padding;    // 16byte揃え
    };


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
    // アウトライン
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOutlinePipelineState_;
    // ラジアルブラー用のパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> radialBlurPipelineState_;

    PostEffectType postEffectType_ = PostEffectType::Copy;

    uint32_t depthSrvIndex_ = 0; // // DepthTextureを読むためのSRV番号
    Microsoft::WRL::ComPtr<ID3D12Resource> outlineParameterResource_; // // Outline用定数バッファ

    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurResource_; // ラジアルブラー用定数バッファ
    RadialBlurData* radialBlurData_ = nullptr; // ラジアルブラー用定数データ

};
