#pragma once
// Windowsのmin/maxマクロと標準ライブラリの関数名が衝突しないようにする。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../renderTexture/RenderTexture.h"
#include <array>
#include <vector>

// タイトルを一枚のガラスとして保存し、シーンをまたいで破片を描く。
class ScreenShatter {
public:
    // 弾痕とヒビを見せてから画面を割るまでの時間。ガラス音の開始にも共通で使う。
    static constexpr float kBreakDelay = 0.40f;
    // 撃ってから弾が画面の着弾点へ届くまでの時間。ヒビや割れる時刻はこの後から数える。
    static constexpr float kBulletFlightTime = 0.12f;
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Start(const Vector2& impactUV);
    void Update(float deltaTime);
    void Capture(ID3D12Resource* source);
    void Draw();
    bool IsPlaying() const { return playing_; }

private:
    // 透視投影後の座標と、元のタイトル画像を切り出すUVを別々に保持する。
    struct Vertex {
        Vector4 position;
        Vector2 uv;
        Vector3 barycentric;
        Vector2 appearance;
    };
    // 外側の大きな破片は多角形になるため、頂点を扇状に三角形分割して描く。
    struct Shard {
        std::vector<Vector2> uv;
        Vector2 center;
        Vector3 velocity;
        Vector3 spin;
        float delay;
    };
    struct Parameters {
        Vector2 impactUV;
        float time;
        float aspect;
        // 割れる瞬間の閃光と、破片の後ろの光の筋を始める時刻
        float breakTime;
        float padding[3];
    };
    // 着弾直後の火花と小片は、大きなガラス片から独立して飛ばす。
    struct ImpactParticle {
        Vector2 velocity;
        float lifetime;
        float size;
        float rotation;
        bool spark;
    };

    void CreatePipeline();
    void CreateShards();
    void UpdateVertices();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::unique_ptr<RenderTexture> snapshot_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertices_;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameters_;
    Vertex* vertexData_ = nullptr;
    Parameters* parameterData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexView_{};
    std::vector<Shard> shards_;
    std::vector<ImpactParticle> impactParticles_;
    UINT vertexCount_ = 0;
    Vector2 impactUV_{ 0.5f, 0.46f };
    float time_ = 0.0f;
    bool playing_ = false;
    bool capturePending_ = false;
};
