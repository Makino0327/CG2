#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <random>
#include <vector>
#include "../../engine/math/Math.h"

class DirectXCommon;

// 画像を使わず、動く帯ポリゴンと煙の粒子でゲームオーバーの背景を作る。
class GameOverEffect {
public:
    void Initialize(DirectXCommon* dxCommon);
    void Update(float seconds);
    void Draw(float opacity = 1.0f);

private:
    struct Tendril {
        Vector2 control[4]{};
        float width = 1.0f;
        float phase = 0.0f;
        float amplitude = 1.0f;
        int parent = -1;
        float attachment = 0.0f;
    };
    struct MistParticle {
        Vector2 position{};
        Vector2 velocity{};
        float age = 0.0f;
        float life = 1.0f;
        float radius = 1.0f;
        float phase = 0.0f;
    };
    struct Vertex {
        Vector2 position;
        Vector2 uv;
        Vector4 color;
        float kind;
    };

    // 枝の付け根は親の曲線へ追従し、一本の生きた筋としてつながる。
    Vector2 SampleTendril(size_t index, float progress) const;
    void ResetParticle(MistParticle& particle, bool prewarm);
    void BuildVertices();
    void AddQuad(const Vector2& center, float radius, const Vector4& color, float kind);
    float Random(float minimum, float maximum);

    static constexpr size_t kMaxVertices = 65536;
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexView_{};
    Vertex* mappedVertices_ = nullptr;
    std::vector<Vertex> vertices_;
    std::vector<Tendril> tendrils_;
    std::vector<MistParticle> mist_;
    // 固定の種で構図を安定させ、粒子の再発生位置だけを順番に変える。
    std::mt19937 randomEngine_{ 271027u };
    float time_ = 0.0f;
};
