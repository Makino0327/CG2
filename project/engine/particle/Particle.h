#pragma once
#include <random>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

#include "../math/Math.h"
#include "../base/srv/SrvManager.h"
#include "Ring.h"

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
    void ShowImGui();

    // エミッターのワールド座標を設定する
    void SetPosition(const Vector3& pos) { emitterPosition_ = pos; }

    // 初期化後にプリセットを切り替える
    void ApplyPreset(ParticleType type);

private:
    // インスタンス数と SRV 関連
    static const uint32_t kNumInstance = 10;
    static const uint32_t kInstancingSrvIndex = 10;

    DirectXCommon* dxCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;
    Camera* camera_ = nullptr;

    // インスタンシング用のリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    // 使用するテクスチャとモデル名
    std::string textureFilePath_;
    std::string modelFileName_;

    // CPU 側で管理するパーティクル配列
    ParticleData          particles_[kNumInstance];
    ParticleEmitterParam  emitterParam_{};
    ParticleType          currentType_ = ParticleType::CircleBurst;

    // エミッターのワールド座標
    Vector3 emitterPosition_{ 0.0f, 0.0f, 0.0f };

    // 発生制御
    bool  isEmitting_ = true;         // パーティクルを発生させるか
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

private:
    ParticleData MakeNewParticle();
    ParticleData MakeDeadParticle();
};
