#pragma once
#include "../DirectX/DirectXCommon.h"
#include "../srv/SrvManager.h"
#include "../renderTexture/RenderTexture.h"
#include <memory>
#include <algorithm>
#include <cmath>

// 個別指定された発光用画像をぼかし、元の画面へ光を加えるBloom処理。
class Bloom {
public:
    // 外部で編集できる見た目の設定だけを受け渡す。
    struct Settings {
        float threshold = 0.0f;
        float knee = 0.0f;
        float intensity = 1.0f;
        float radius = 1.2f;
    };

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    // 入力画像は読み取り状態、出力先は描画状態にして呼び出す。
    void Draw(uint32_t inputSrvIndex, uint32_t emissionSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRTV);

    // 編集用のコピーを返し、内部の描画用データを直接操作させない。
    Settings GetSettings() const {
        return { parameters_.threshold, parameters_.knee, parameters_.intensity, parameters_.radius };
    }
    // 許可した範囲の有限値だけを反映し、描画方向やパディングを保護する。
    void SetSettings(const Settings& settings) {
        if (std::isfinite(settings.threshold)) { parameters_.threshold = std::clamp(settings.threshold, 0.0f, 1.0f); }
        if (std::isfinite(settings.knee)) { parameters_.knee = std::clamp(settings.knee, 0.0f, 0.5f); }
        if (std::isfinite(settings.intensity)) { parameters_.intensity = std::clamp(settings.intensity, 0.0f, 6.0f); }
        if (std::isfinite(settings.radius)) { parameters_.radius = std::clamp(settings.radius, 0.5f, 5.0f); }
    }

private:
    // GPUへ渡す配置と描画方向はBloom内部だけで管理する。
    struct Parameters {
        float threshold = 0.0f;  // 指定された光を全て使う。弱い光を除きたい時だけ上げる。
        float knee = 0.0f;       // しきい値を使う場合の、滑らかな切り替え幅。
        float intensity = 1.0f;  // 周囲の光を控えめに足し、明るい芯を見せる。
        float radius = 1.2f;     // にじみを狭くして、弾が太く見えすぎるのを抑える。
        Vector2 direction{};     // 横・縦のぼかし方向。描画するたびに設定する。
        Vector2 padding{};       // HLSLの定数配置に合わせる。
    };

    void CreatePipeline();
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    // ぼかしを軽くするため、明るい部分だけを半分の解像度で保持する。
    std::unique_ptr<RenderTexture> brightTexture_;
    std::unique_ptr<RenderTexture> blurTexture_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> extractPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> blurPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipeline_;
    Parameters parameters_{};
};
