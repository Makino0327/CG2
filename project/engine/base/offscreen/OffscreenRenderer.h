#pragma once
#define NOMINMAX
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
    Dissolve,
    RandomNoise,
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

    void Update(float deltaTime);

    void StartDissolve();

    void SetDissolveDuration(float seconds) { dissolveDuration_ = seconds; }
    float GetDissolveDuration() const { return dissolveDuration_; }

    void SetDissolveMaskType(int type) { dissolveMaskType_ = type; }
    int GetDissolveMaskType() const { return dissolveMaskType_; }

    void SetDissolveElapsedTime(float seconds);
    float GetDissolveElapsedTime() const { return dissolveElapsedTime_; }

    bool IsDissolvePlaying() const { return isDissolvePlaying_; }
    float GetDissolveThreshold() const { return dissolveData_ ? dissolveData_->threshold : 0.0f; }

    void DrawImGui();

private:
    struct RadialBlurData {
        Vector2 center;   // ブラーの中心UV
        float blurWidth;  // ブラーの強さ
        float padding;    // 16byte揃え
    };

    struct DissolveData {
        float threshold;   // ディゾルブの進行度
        float edgeWidth;   // 境界の幅
        Vector2 padding;   // 16byte揃え
        Vector4 edgeColor; // 境界色
    };

    struct RandomNoiseData {
        float intensity;  // ノイズの濃さ
        float time;       // 乱数変化用の時間
        float speed;      // ノイズ変化の速さ
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
    // ディゾルブ用のパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> dissolvePipelineState_;
    // ランダムノイズ用のパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> randomNoisePipelineState_;

    PostEffectType postEffectType_ = PostEffectType::Copy;

    uint32_t depthSrvIndex_ = 0; // // DepthTextureを読むためのSRV番号
    Microsoft::WRL::ComPtr<ID3D12Resource> outlineParameterResource_; // // Outline用定数バッファ

    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurResource_; // ラジアルブラー用定数バッファ
    RadialBlurData* radialBlurData_ = nullptr; // ラジアルブラー用定数データ

    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveResource_; // ディゾルブ用定数バッファ
    DissolveData* dissolveData_ = nullptr; // ディゾルブ用定数データ

    uint32_t dissolveMaskSrvIndex0_ = 0; // noise0用SRV
    uint32_t dissolveMaskSrvIndex1_ = 0; // noise1用SRV
    int dissolveMaskType_ = 0; // 0: noise0 1: noise1

    float dissolveDuration_ = 2.0f; // 何秒で終わるか
    float dissolveElapsedTime_ = 0.0f; // 経過時間
    bool isDissolvePlaying_ = false; // 再生中かどうか

    Microsoft::WRL::ComPtr<ID3D12Resource> randomNoiseResource_; // ランダムノイズ用定数バッファ
    RandomNoiseData* randomNoiseData_ = nullptr; // ランダムノイズ用定数データ


};
