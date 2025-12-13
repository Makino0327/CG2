#pragma once
#include <random>
#include <string>
#include <d3d12.h>
#include <wrl.h>

#include "Math.h"
#include "SrvManager.h"

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
// CPU側1粒のデータ
// =============================
struct ParticleData {
    Transform transform;
    Vector3   velocity;
    Vector4   color;
    float     lifeTime;
    float     currentTime;
};

// =============================
// エミッタのパラメータ
// =============================
struct ParticleEmitterParam
{
    float   positionRange;   // どのくらいの範囲にばらまくか
    float   velocityRange;   // どのくらいの速さで飛ばすか
    float   lifeTimeMin;     // 寿命の最小
    float   lifeTimeMax;     // 寿命の最大
    Vector4 baseColor;       // 基本の色
    bool    randomColor;     // ランダム色を使うか
};

// =============================
// GPU に送る1粒分
// =============================
struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4   color;
};

// =============================
// プリセット1件分
// =============================
struct ParticlePreset {
    ParticleType          type;
    const char* name;        // "CircleBurst" など
    const char* texturePath; // "Resources/circle.png" など
    const char* modelName;   // "plane.obj" など
    ParticleEmitterParam  param;
};

// =============================
// パーティクルシステム本体
// =============================
class ParticleSystem
{
public:
    // 初期化：どのプリセットを使うかだけ指定
    void Initialize(DirectXCommon* dxCommon,
        ParticleCommon* particleCommon,
        Camera* camera,
        SrvManager* srvManager,
        ParticleType type);

    // 毎フレーム更新
    void Update(float deltaTime);

    // 描画
    void Draw();

    // ImGui ウィンドウ
    void ShowImGui();

    // エミッタの基準位置（左手座標系）
    void SetPosition(const Vector3& pos) { emitterPosition_ = pos; }

    // あとから種類を切り替える
    void ApplyPreset(ParticleType type);

private:
    // インスタンス数と、SRVヒープで使うインデックス
    static const uint32_t kNumInstance = 10;
    static const uint32_t kInstancingSrvIndex = 10;

    DirectXCommon* dxCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;
    Camera* camera_ = nullptr;

    // Instancing用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    // テクスチャ / モデル
    std::string textureFilePath_;
    std::string modelFileName_;

    // CPU 側パーティクル
    ParticleData          particles_[kNumInstance];
    ParticleEmitterParam  emitterParam_{};
    ParticleType          currentType_ = ParticleType::CircleBurst;

    // エミッタのワールド位置
    Vector3 emitterPosition_{ 0.0f, 0.0f, 0.0f };
    // ===== 追加：放出（Emit）制御 =====
    bool  isEmitting_ = true;         // 出し続けるか
    float emitRate_ = 30.0f;          // 1秒あたり何個出すか
    float emitAccumulator_ = 0.0f;    // 端数の蓄積
    uint32_t emitCursor_ = 0;         // 次に上書きする粒の番号（リング）


    // ===== 風（CircleBurst用）=====
    bool  windEnabled_ = true;
    Vector3 windDirection_{ 1.0f, 0.0f, 0.0f }; // 右方向がデフォ（+X）
    float windStrength_ = 20.0f;                // 風の強さ
    bool  windAsAcceleration_ = true;          // true=加速度（自然） / false=速度に直接足す


    // 乱数
    std::mt19937 randomEngine_;

    SrvManager* srvManager_ = nullptr;        // ★追加
    uint32_t instancingSrvIndex_ = 0;

private:
    ParticleData MakeNewParticle();
};
