#pragma once
#include <random>
#include <string>
#include <d3d12.h>
#include <wrl.h>

#include "../math/Math.h"
#include "../base/srv/SrvManager.h"

class DirectXCommon;
class ParticleCommon;
class Camera;
class Model;

// =============================
// Particle type
// =============================
enum class ParticleType {
    CircleBurst = 0,
    Explosion = 1,
    Smoke = 2,
};

// =============================
// CPU-side data for one particle
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
// Emitter parameters
// =============================
struct ParticleEmitterParam
{
    float   positionRange;   // Spawn spread range. CircleBurst does not use this now.
    float   velocityRange;   // Random velocity range.
    float   lifeTimeMin;     // Minimum lifetime.
    float   lifeTimeMax;     // Maximum lifetime.
    Vector4 baseColor;       // Base color.
    bool    randomColor;     // Use random color.
};

// =============================
// GPU-side data for one particle
// =============================
struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4   color;
};

// =============================
// One particle preset
// =============================
struct ParticlePreset {
    ParticleType          type;
    const char* name;
    const char* texturePath;
    const char* modelName;
    ParticleEmitterParam  param;
};

// =============================
// Particle system
// =============================
class ParticleSystem
{
public:
    // Initialize particle system with a preset.
    void Initialize(DirectXCommon* dxCommon,
        ParticleCommon* particleCommon,
        Camera* camera,
        SrvManager* srvManager,
        ParticleType type);

    // Update particles every frame.
    void Update(float deltaTime);

    // Draw particles.
    void Draw();

    // Show ImGui editor.
    void ShowImGui();

    // Set emitter world position.
    void SetPosition(const Vector3& pos) { emitterPosition_ = pos; }

    // Change preset after initialization.
    void ApplyPreset(ParticleType type);

private:
    // Instance count and SRV index.
    static const uint32_t kNumInstance = 10;
    static const uint32_t kInstancingSrvIndex = 10;

    DirectXCommon* dxCommon_ = nullptr;
    ParticleCommon* particleCommon_ = nullptr;
    Camera* camera_ = nullptr;

    // Instancing resource.
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    // Texture and model.
    std::string textureFilePath_;
    std::string modelFileName_;

    // CPU-side particles.
    ParticleData          particles_[kNumInstance];
    ParticleEmitterParam  emitterParam_{};
    ParticleType          currentType_ = ParticleType::CircleBurst;

    // Emitter world position.
    Vector3 emitterPosition_{ 0.0f, 0.0f, 0.0f };

    // Emit control.
    bool  isEmitting_ = true;         // Particle emission on/off.
    float emitInterval_ = 1.0f;       // Seconds between burst emissions.
    float emitTimer_ = 0.0f;          // Elapsed time for the next burst.
    int   emitCount_ = 3;             // Particles emitted per burst.
    uint32_t emitCursor_ = 0;         // Next particle index to overwrite.

    // Random generator.
    std::mt19937 randomEngine_;

    SrvManager* srvManager_ = nullptr;
    uint32_t instancingSrvIndex_ = 0;

private:
    ParticleData MakeNewParticle();
    ParticleData MakeDeadParticle();
};
