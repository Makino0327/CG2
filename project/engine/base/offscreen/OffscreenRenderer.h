#pragma once
#define NOMINMAX
#include "../DirectX/DirectXCommon.h"
#include "../srv/SrvManager.h"
#include "../renderTexture/RenderTexture.h"
#include <array>

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
    Shockwave,
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
    void SetPostEffectEnabled(PostEffectType type, bool enabled);
    bool IsPostEffectEnabled(PostEffectType type) const;
    void SetVignetteIntensity(float intensity);

    // Input を受け取って Game View 上のマウス情報を毎フレーム更新する
    void Update(float deltaTime, const Vector2& mousePosition, bool isMouseRightPressed);

    void StartDissolve();

    // 指定した画面UVを中心に衝撃波の歪みを開始する
    void StartShockwave(const Vector2& centerUV);
    // 今回だけ最大サイズを指定して衝撃波を出す
    void StartShockwave(const Vector2& centerUV, float maxRadius);

    // ゲームシーンから衝撃波の最大サイズを変更する
    void SetShockwaveMaxRadius(float radius) { shockwaveMaxRadius_ = radius; }
    float GetShockwaveMaxRadius() const { return shockwaveMaxRadius_; }

    // 白い波動を表示するかどうかを切り替える
    void SetShockwaveWhiteWaveEnabled(bool enabled) { shockwaveWhiteWaveEnabled_ = enabled; if (shockwaveData_) { shockwaveData_->whiteWave = enabled ? 1.0f : 0.0f; } }
    bool IsShockwaveWhiteWaveEnabled() const { return shockwaveWhiteWaveEnabled_; }

    void SetDissolveDuration(float seconds) { dissolveDuration_ = seconds; }
    float GetDissolveDuration() const { return dissolveDuration_; }

    void SetDissolveMaskType(int type) { dissolveMaskType_ = type; }
    int GetDissolveMaskType() const { return dissolveMaskType_; }

    void SetDissolveElapsedTime(float seconds);
    float GetDissolveElapsedTime() const { return dissolveElapsedTime_; }

    bool IsDissolvePlaying() const { return isDissolvePlaying_; }
    float GetDissolveThreshold() const { return dissolveData_ ? dissolveData_->threshold : 0.0f; }

    // Game View 内でのローカル座標を返す
    Vector2 GetGameViewMousePosition() const { return gameViewMousePosition_; }

    // Game View 内で 0.0f ～ 1.0f に正規化した座標を返す
    Vector2 GetGameViewMouseUV() const { return gameViewMouseUV_; }

    // Game Viewの画像が表示されている左上座標を返す
    Vector2 GetGameViewTopLeft() const { return gameViewTopLeft_; }

    // Game Viewの画像が表示されているサイズを返す
    Vector2 GetGameViewSize() const { return gameViewSize_; }

    void DrawImGui();

    // デバッグ用にゲーム画面をImGuiへ表示する
    void DrawDebugGameViewImGui();

    // Game Viewがホバー中か
    bool IsGameViewHovered() const { return isGameViewHovered_; }

    // Game View が選択中かどうか
    bool IsGameViewSelected() const { return isGameViewSelected_; }

    // Game View上で右ドラッグ中か
    bool IsGameViewRotating() const { return isGameViewRotating_; }

    // Game View上でのマウス移動量
    Vector2 GetGameViewMouseDelta() const { return gameViewMouseDelta_; }


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

    struct VignetteData {
        float intensity; // ビネットの強さ
        Vector3 padding; // 16byte境界に合わせる
    };

    struct ShockwaveData {
        Vector2 center;    // 歪みの中心UV
        float radius;      // 衝撃波リングの半径
        float thickness;   // 衝撃波リングの太さ
        float strength;    // UVをずらす強さ
        float progress;    // 再生進行度
        float aspectRatio; // 画面比率補正
        float whiteWave;   // 白い波動を表示するかどうか
    };


private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<RenderTexture> renderTexture_;
    std::unique_ptr<RenderTexture> workRenderTexture_; // 複数のポストエフェクトを順番にかけるための作業用テクスチャ

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
    // 発射時の空気の歪み用パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shockwavePipelineState_;

    PostEffectType postEffectType_ = PostEffectType::Copy;
    std::array<bool, static_cast<size_t>(PostEffectType::DepthOutline) + 1> enabledPostEffects_{}; // 同時に有効化するポストエフェクト一覧

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

    Microsoft::WRL::ComPtr<ID3D12Resource> vignetteResource_; // ビネット用定数バッファ
    VignetteData* vignetteData_ = nullptr; // ビネット用定数データ

    Microsoft::WRL::ComPtr<ID3D12Resource> shockwaveResource_; // 衝撃波歪み用定数バッファ
    ShockwaveData* shockwaveData_ = nullptr; // 衝撃波歪み用定数データ
    float shockwaveDuration_ = 0.28f; // 衝撃波が広がりきるまでの時間
    float shockwaveMaxRadius_ = 0.15f; // 衝撃波が最大で広がる大きさ
    float currentShockwaveMaxRadius_ = 0.15f; // 再生中の衝撃波だけに使う最大サイズ
    bool shockwaveWhiteWaveEnabled_ = true; // 白い波動を表示するかどうか
    float shockwaveElapsedTime_ = 0.0f; // 衝撃波の経過時間
    bool isShockwavePlaying_ = false; // 衝撃波の再生中フラグ
    PostEffectType shockwaveReturnType_ = PostEffectType::Copy; // 衝撃波終了後に戻すエフェクト

    // Game Viewの入力状態
    bool isGameViewHovered_ = false;
    bool isGameViewSelected_ = false;
    bool isGameViewRotating_ = false;
    Vector2 gameViewMouseDelta_ = { 0.0f, 0.0f };

    // Game View の表示矩形
    Vector2 gameViewTopLeft_ = { 0.0f, 0.0f };
    Vector2 gameViewSize_ = { 0.0f, 0.0f };

    // Game View 内でのマウス座標
    Vector2 gameViewMousePosition_ = { 0.0f, 0.0f };

    // Game View 内での正規化マウス座標
    Vector2 gameViewMouseUV_ = { 0.0f, 0.0f };

    // 前フレームの Game View 内マウス座標
    Vector2 prevGameViewMousePosition_ = { 0.0f, 0.0f };

    // 右ドラッグで視点回転を開始したかどうかを保持する
    bool isGameViewRotationStarted_ = false;
};
