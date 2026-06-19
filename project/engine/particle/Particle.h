#pragma once
#include <random>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

#include "../math/Math.h"
#include "../base/srv/SrvManager.h"
#include "Ring.h"
#include "Cylinder.h"
#include "ParticleCommon.h"

class DirectXCommon;
class ParticleCommon;
class Camera;
class Model;

// =============================
// パーティクルの種類
// =============================
enum class ParticleType {
    CircleBurst = 0,
    Explosion = 1,
    Smoke = 2,
};

// =============================
// 描画に使う形状の種類
// =============================
enum class EffectMeshType {
    Plane = 0,
    Ring = 1,
    Cylinder = 2,
};

// =============================
// パーティクル1個分のCPU側データ
// =============================
struct ParticleData {
    Transform transform;
    Vector3   velocity;
    Vector4   color;
    float     lifeTime;
    float     currentTime;
    bool      isAlive;
};

// ComputeShader で扱う Particle の本体データ
struct ParticleCS {
    Vector3 translate;
    Vector3 scale;
    float lifeTime;
    Vector3 velocity;
    float currentTime;
    Vector4 color;
};

// GPUで発生処理に使うエミッター
struct EmitterSphere {
    Vector3 translate;      // 発生中心
    float radius;           // 発生半径

    uint32_t count;         // 1回で発生させる数
    float frequency;        // 発生間隔
    float frequencyTime;    // 経過時間
    uint32_t emit;          // 今フレーム発生するか
};

// ComputeShaderへ毎フレーム送る時間情報
struct PerFrame {
    float time;             // 累積時間
    float deltaTime;        // 1フレームの時間
};


// =============================
// エミッターの設定値
// =============================
struct ParticleEmitterParam
{
    float   positionRange;   // 発生位置のばらつき範囲。今は CircleBurst では未使用
    float   velocityRange;   // 速度のランダム範囲
    float   lifeTimeMin;     // 生存時間の最小値
    float   lifeTimeMax;     // 生存時間の最大値
    Vector4 baseColor;       // 基本色
    bool    randomColor;     // ランダム色を使うか
};

// =============================
// パーティクル1個分のGPU側データ
// =============================
struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4   color;
};

// VertexShader に渡すビュー用データ
struct ParticlePerView {
    Matrix4x4 viewProjection;
    Matrix4x4 billboardMatrix;
};


// =============================
// パーティクルのプリセット
// =============================
struct ParticlePreset {
    ParticleType          type;
    const char* name;
    const char* texturePath;
    const char* modelName;
    ParticleEmitterParam  param;
};

// =============================
// パーティクルシステム
// =============================
class ParticleSystem
{
public:
    // プリセットを指定してパーティクルシステムを初期化する
    void Initialize(DirectXCommon* dxCommon,
        ParticleCommon* particleCommon,
        Camera* camera,
        SrvManager* srvManager,
        ParticleType type);

    // 毎フレーム更新する
    void Update(float deltaTime);

    // パーティクルを描画する
    void Draw();

    // ImGui の編集UIを表示する
    void ShowImGui(const char* windowName);
    void SetCamera(Camera* camera) { camera_ = camera; }

    // パーティクルシステム単位で描画時の合成方式を設定する
    void SetBlendMode(ParticleBlendMode blendMode) { blendMode_ = blendMode; }


    // エミッターのワールド座標を設定する
    void SetPosition(const Vector3& pos) { emitterPosition_ = pos; }

    // 任意の位置へ手動でパーティクルを1個生成する
    bool Emit(
        const Vector3& position,
        const Vector3& scale,
        const Vector3& velocity,
        const Vector4& color,
        float lifeTime);

    // 初期化後にプリセットを切り替える
    void ApplyPreset(ParticleType type);

    // // 描画形状を切り替える
    void SetMeshType(EffectMeshType type);


private:
    // インスタンス数と SRV 関連
    static const uint32_t kNumInstance = 1024;
    static const uint32_t kInstancingSrvIndex = 10;

    // 手動生成したパーティクルのCPU側管理情報
    struct ManualParticle {
        // GPUへ送るパーティクル情報
        ParticleCS gpuData{};

        // フェード前の初期サイズ
        Vector3 initialScale{ 0.0f, 0.0f, 0.0f };

        // フェード前の初期色
        Vector4 initialColor{ 1.0f, 1.0f, 1.0f, 0.0f };

        // 現在使用中か
        bool isAlive = false;
    };

    DirectXCommon* dxCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;
    Camera* camera_ = nullptr;

    // インスタンシング用のリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    // GPU Particle 本体を保持する Resource
    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;

    // 手動パーティクルをVertexShaderへ渡すStructuredBuffer
    Microsoft::WRL::ComPtr<ID3D12Resource> manualParticleResource_;

    // Mapした手動パーティクルバッファの書き込み先
    ParticleCS* manualParticleData_ = nullptr;

    // 手動パーティクル用SRV番号
    uint32_t manualParticleSrvIndex_ = 0;

    // 手動パーティクル用SRVのGPUハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE manualParticleSrvHandleGPU_{};

    // 手動生成したパーティクル一覧
    ManualParticle manualParticles_[kNumInstance]{};

    // 次に空きを探し始める配列位置
    uint32_t manualEmitCursor_ = 0;

    // GPU Particle 用の SRV index
    uint32_t particleSrvIndex_ = 0;

    // GPU Particle 用の UAV index
    uint32_t particleUavIndex_ = 0;

    // GPU Particle 用の SRV の GPU ハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_{};

    // GPU Particle 用の UAV の GPU ハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_{};

    // VertexShader に渡す PerView 用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;

    // PerView 用定数バッファの書き込み先
    ParticlePerView* perViewData_ = nullptr;


    // 使用するテクスチャとモデル名
    std::string textureFilePath_;
    std::string modelFileName_;

    // CPU 側で管理するパーティクル配列
    ParticleData          particles_[kNumInstance];
    ParticleEmitterParam  emitterParam_{};
    ParticleType          currentType_ = ParticleType::CircleBurst;

    // 初期状態は発光エフェクト向けの加算合成を使用する
    ParticleBlendMode blendMode_ = ParticleBlendMode::Additive;

    // エミッターのワールド座標
    Vector3 emitterPosition_{ 0.0f, 0.0f, 0.0f };

    // 発生制御
    bool  isEmitting_ = false;         // パーティクルを発生させるか
    float emitInterval_ = 1.0f;       // 発生間隔（秒）
    float emitTimer_ = 0.0f;          // 次の発生までの経過時間
    int   emitCount_ = 3;             // 1回の発生で出す個数
    uint32_t emitCursor_ = 0;         // 次に上書きするパーティクル番号

    // 乱数生成器
    std::mt19937 randomEngine_;

    SrvManager* srvManager_ = nullptr;
    uint32_t instancingSrvIndex_ = 0;

    // 描画に使う形状
    EffectMeshType meshType_ = EffectMeshType::Plane; // 現在使う形状
    std::unique_ptr<Ring> ring_;                      // Ring 描画用オブジェクト
    // // Cylinder 描画用オブジェクト
    std::unique_ptr<Cylinder> cylinder_;

    // GPU発生用エミッター本体
    EmitterSphere emitterSphere_{};

    // GPU発生用の時間情報
    PerFrame perFrameForCS_{};

    // Emitter用ConstantBuffer
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource_;
    EmitterSphere* emitterData_ = nullptr;

    // PerFrame用ConstantBuffer
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResourceForCS_;
    PerFrame* perFrameDataForCS_ = nullptr;

    // FreeListの先頭Indexを保持する
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
    uint32_t freeListIndexUavIndex_ = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE freeListIndexUavHandleGPU_{};

    // 空いているParticleIndexそのものを並べて保持する
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
    uint32_t freeListUavIndex_ = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE freeListUavHandleGPU_{};

private:
    ParticleData MakeNewParticle();
    ParticleData MakeDeadParticle();

    // GPU Particle 用 Resource と View を作る
    void InitializeGPUParticleResource();

    // 手動パーティクル用StructuredBufferを作る
    void InitializeManualParticleResource();

    // 手動生成したパーティクルを更新する
    void UpdateManualParticles(float deltaTime);

    // 指定した手動パーティクルを非表示にする
    void KillManualParticle(uint32_t index);

    // VertexShader 用の PerView 定数バッファを作る
    void InitializePerViewResource();

    // ComputeShader で Particle を初期化する
    void InitializeParticleCS();

    // GPU発生用EmitterのConstantBufferを作る
    void InitializeEmitterResource();

    // GPU発生用PerFrameのConstantBufferを作る
    void InitializePerFrameResourceForCS();

    // GPU発生用FreeListのUAVを作る
    void InitializeFreeListResource();

    // 毎フレームGPUでParticleを発生させる
    void DispatchEmitParticleCS();

    // 毎フレームGPUでParticleを更新する
    void DispatchUpdateParticleCS();

};
