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

    // ポストエフェクトの種類を切り替える
    void SetPostEffectType(PostEffectType type) { postEffectType_ = type; }
    PostEffectType GetPostEffectType() const { return postEffectType_; }

    // 開始演出用のぼかし強度を設定する
    void SetBlurStrength(float strength);

    // Input を受け取って Game View の状態を更新する
    void Update(float deltaTime, const Vector2& mousePosition, bool isMouseRightPressed);

    void StartDissolve();

    void SetDissolveDuration(float seconds) { dissolveDuration_ = seconds; }
    float GetDissolveDuration() const { return dissolveDuration_; }

    void SetDissolveMaskType(int type) { dissolveMaskType_ = type; }
    int GetDissolveMaskType() const { return dissolveMaskType_; }

    void SetDissolveElapsedTime(float seconds);
    float GetDissolveElapsedTime() const { return dissolveElapsedTime_; }

    bool IsDissolvePlaying() const { return isDissolvePlaying_; }
    float GetDissolveThreshold() const { return dissolveData_ ? dissolveData_->threshold : 0.0f; }

    // Game View 上でのローカルマウス座標を返す
    Vector2 GetGameViewMousePosition() const { return gameViewMousePosition_; }

    // Game View 上で 0.0f から 1.0f に正規化したマウス座標を返す
    Vector2 GetGameViewMouseUV() const { return gameViewMouseUV_; }

    void DrawImGui();

    // デバッグ用にゲーム画面を ImGui に表示する
    void DrawDebugGameViewImGui();

    // Game View の上にマウスがあるか
    bool IsGameViewHovered() const { return isGameViewHovered_; }

    // Game View が選択中かどうか
    bool IsGameViewSelected() const { return isGameViewSelected_; }

    // Game View 上で右ドラッグ中かどうか
    bool IsGameViewRotating() const { return isGameViewRotating_; }

    // Game View 上でのマウス移動量
    Vector2 GetGameViewMouseDelta() const { return gameViewMouseDelta_; }

private:
    struct RadialBlurData {
        Vector2 center;   // ブラーの中心UV
        float blurWidth;  // ブラーの幅
        float padding;    // 16byte境界に揃える
    };

    struct DissolveData {
        float threshold;   // ディゾルブの進行度
        float edgeWidth;   // エッジの幅
        Vector2 padding;   // 16byte境界に揃える
        Vector4 edgeColor; // エッジ色
    };

    struct RandomNoiseData {
        float intensity;  // ノイズの強さ
        float time;       // 時間変化用の時間
        float speed;      // ノイズ変化の速さ
        float padding;    // 16byte境界に揃える
    };

    struct BlurData {
        float strength;   // ぼかしの強さ
        Vector3 padding;  // 16byte境界に揃える
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
    Microsoft::WRL::ComPtr<ID3D12PipelineState> vignettePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> boxFilterPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianFilterPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOutlinePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> radialBlurPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> dissolvePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> randomNoisePipelineState_;

    PostEffectType postEffectType_ = PostEffectType::Copy;

    uint32_t depthSrvIndex_ = 0; // DepthTexture 用の SRV 番号
    Microsoft::WRL::ComPtr<ID3D12Resource> outlineParameterResource_; // Outline 用定数バッファ

    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurResource_; // RadialBlur 用定数バッファ
    RadialBlurData* radialBlurData_ = nullptr; // RadialBlur 用パラメータ

    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveResource_; // Dissolve 用定数バッファ
    DissolveData* dissolveData_ = nullptr; // Dissolve 用パラメータ

    uint32_t dissolveMaskSrvIndex0_ = 0; // noise0 用 SRV
    uint32_t dissolveMaskSrvIndex1_ = 0; // noise1 用 SRV
    int dissolveMaskType_ = 0; // 0: noise0 1: noise1

    float dissolveDuration_ = 2.0f; // 演出全体の時間
    float dissolveElapsedTime_ = 0.0f; // 経過時間
    bool isDissolvePlaying_ = false; // 再生中かどうか

    Microsoft::WRL::ComPtr<ID3D12Resource> randomNoiseResource_; // RandomNoise 用定数バッファ
    RandomNoiseData* randomNoiseData_ = nullptr; // RandomNoise 用パラメータ

    Microsoft::WRL::ComPtr<ID3D12Resource> blurResource_; // 開始ぼかし用の定数バッファ
    BlurData* blurData_ = nullptr; // 開始ぼかし用のパラメータ

    // Game View の入力状態
    bool isGameViewHovered_ = false;
    bool isGameViewSelected_ = false;
    bool isGameViewRotating_ = false;
    Vector2 gameViewMouseDelta_ = { 0.0f, 0.0f };

    // Game View の表示領域
    Vector2 gameViewTopLeft_ = { 0.0f, 0.0f };
    Vector2 gameViewSize_ = { 0.0f, 0.0f };

    // Game View 上のマウス座標
    Vector2 gameViewMousePosition_ = { 0.0f, 0.0f };

    // Game View 上の正規化マウス座標
    Vector2 gameViewMouseUV_ = { 0.0f, 0.0f };

    // 1フレーム前の Game View 上のマウス座標
    Vector2 prevGameViewMousePosition_ = { 0.0f, 0.0f };

    // 右ドラッグ開始後に回転状態へ入ったかどうか
    bool isGameViewRotationStarted_ = false;
};
