#include "Particle.h"

#include "../base/DirectX/DirectXCommon.h"
#include "ParticleCommon.h"
#include "../2d/texture/TextureManager.h"
#include "../3d/model/ModelManager.h"
#include "../../game/camera/Camera.h"
#include "../math/Math.h"
#include <cassert>
#include <cmath>
#include <numbers>

#include "../externals/imgui/imgui.h"
#include "../externals/imgui/imgui_impl_win32.h"
#include "../externals/imgui/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;

// =============================
// Particle preset list
// =============================
static const ParticlePreset kParticlePresets[] = {
    {
        ParticleType::CircleBurst,
        "CircleBurst",
        "Resources/circle2.png",
        "plane.obj",
        { 0.0f, 0.0f, 2.0f, 2.0f, {1,1,1,1}, false }
    },
    {
        ParticleType::Explosion,
        "Explosion",
        "Resources/circle2.png",
        "plane.obj",
        { 2.0f, 4.0f, 0.3f, 0.8f, {1,0.5f,0.2f,1}, true }
    },
    {
        ParticleType::Smoke,
        "Smoke",
        "Resources/circle2.png",
        "plane.obj",
        { 1.5f, 0.5f, 1.0f, 3.0f, {0.4f,0.4f,0.4f,1}, false }
    },
};

static const ParticlePreset& GetPreset(ParticleType type)
{
    for (const auto& preset : kParticlePresets) {
        if (preset.type == type) {
            return preset;
        }
    }

    // Use the first preset when the requested type is not found.
    return kParticlePresets[0];
}

void ParticleSystem::Initialize(
    DirectXCommon* dxCommon,
    ParticleCommon* particleCommon,
    Camera* camera,
    SrvManager* srvManager,
    ParticleType type)
{
    dxCommon_ = dxCommon;
    particleCommon_ = particleCommon;
    camera_ = camera;
    srvManager_ = srvManager;

    std::random_device seed;
    randomEngine_ = std::mt19937(seed());

    // Apply texture, model, and parameters from the preset.
    ApplyPreset(type);

    // Create the GPU buffer used for instancing.
    ID3D12Device* device = dxCommon_->GetDevice();
    instancingResource_ =
        dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumInstance);

    instancingResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&instancingData_));

    for (uint32_t i = 0; i < kNumInstance; ++i) {
        instancingData_[i].WVP = MakeIdentity4x4();
        instancingData_[i].World = MakeIdentity4x4();
        instancingData_[i].color = Vector4(1, 1, 1, 0);
    }

    // Register the instancing buffer as an SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kNumInstance;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    assert(srvManager_->CanAllocate());
    instancingSrvIndex_ = srvManager_->Allocate();

    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
        srvManager_->GetCPUDescriptorHandle(instancingSrvIndex_);
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
        srvManager_->GetGPUDescriptorHandle(instancingSrvIndex_);

    device->CreateShaderResourceView(
        instancingResource_.Get(), &srvDesc, handleCPU);

    instancingSrvHandleGPU_ = handleGPU;

    // Start with invisible particles and emit them later.
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        particles_[i] = MakeDeadParticle();
    }
}

void ParticleSystem::ApplyPreset(ParticleType type)
{
    currentType_ = type;
    const ParticlePreset& preset = GetPreset(type);

    emitterParam_ = preset.param;
    modelFileName_ = preset.modelName;

    // Load the texture used by this preset.
    TextureManager* texMan = TextureManager::GetInstance();
    texMan->LoadTexture(preset.texturePath);
    textureFilePath_ = preset.texturePath;

    // Clear old particles when the preset changes.
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        particles_[i] = MakeDeadParticle();
    }
}

ParticleData ParticleSystem::MakeNewParticle()
{
    // Prepare random ranges.
    std::uniform_real_distribution<float> distVel(
        -emitterParam_.velocityRange, emitterParam_.velocityRange);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(
        emitterParam_.lifeTimeMin, emitterParam_.lifeTimeMax);
    std::uniform_real_distribution<float> distScale(0.3f, 1.2f);
    std::uniform_real_distribution<float> distRotate(
        -std::numbers::pi_v<float>,
        std::numbers::pi_v<float>);

    ParticleData p{};

    // Make a thin vertical streak instead of a round particle.
    float scale = distScale(randomEngine_);
    p.transform.scale = { scale * 0.15f, scale * 1.8f, scale };

    // Randomize only the rotation so particles overlap at the same position.
    p.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine_) };

    // Spawn exactly at the emitter position.
    p.transform.translate = emitterPosition_;

    switch (currentType_) {
    case ParticleType::CircleBurst:
        // Keep CircleBurst particles at the emitter position.
        p.velocity = { 0.0f, 0.0f, 0.0f };
        break;

    case ParticleType::Explosion:
        // Push Explosion particles outward.
        p.velocity = {
            distVel(randomEngine_) * 2.0f,
            distVel(randomEngine_) * 2.0f,
            distVel(randomEngine_) * 2.0f
        };
        break;

    case ParticleType::Smoke:
        // Move Smoke particles slowly upward.
        p.velocity = {
            distVel(randomEngine_) * 0.2f,
            std::fabs(distVel(randomEngine_)) * 0.5f,
            distVel(randomEngine_) * 0.2f
        };
        break;
    }

    // Set particle color.
    if (emitterParam_.randomColor) {
        p.color = {
            distColor(randomEngine_),
            distColor(randomEngine_),
            distColor(randomEngine_),
            1.0f
        };
    } else {
        p.color = emitterParam_.baseColor;
    }

    // Set lifetime.
    p.lifeTime = distTime(randomEngine_);
    p.currentTime = 0.0f;
    p.isAlive = true;

    return p;
}

ParticleData ParticleSystem::MakeDeadParticle()
{
    ParticleData p{};

    // Hide unused particles because DrawInstanced uses a fixed count.
    p.transform.scale = { 0.0f, 0.0f, 0.0f };
    p.transform.rotate = { 0.0f, 0.0f, 0.0f };
    p.transform.translate = emitterPosition_;
    p.velocity = { 0.0f, 0.0f, 0.0f };
    p.color = { 1.0f, 1.0f, 1.0f, 0.0f };
    p.lifeTime = 1.0f;
    p.currentTime = 1.0f;
    p.isAlive = false;

    return p;
}

void ParticleSystem::Update(float deltaTime)
{
    if (!camera_) { return; }

    // Emit several particles at once from the same position.
    if (isEmitting_) {
        emitTimer_ += deltaTime;

        if (emitTimer_ >= emitInterval_) {
            emitTimer_ = 0.0f;

            for (int i = 0; i < emitCount_; ++i) {
                particles_[emitCursor_] = MakeNewParticle();
                emitCursor_ = (emitCursor_ + 1) % kNumInstance;
            }
        }
    }

    for (uint32_t i = 0; i < kNumInstance; ++i) {
        ParticleData& p = particles_[i];

        // Keep inactive particles transparent.
        if (!p.isAlive) {
            instancingData_[i].World = MakeIdentity4x4();
            instancingData_[i].WVP = MakeIdentity4x4();
            instancingData_[i].color = { 1.0f, 1.0f, 1.0f, 0.0f };
            continue;
        }

        // Hide expired particles.
        p.currentTime += deltaTime;
        if (p.currentTime > p.lifeTime) {
            p = MakeDeadParticle();
            instancingData_[i].World = MakeIdentity4x4();
            instancingData_[i].WVP = MakeIdentity4x4();
            instancingData_[i].color = { 1.0f, 1.0f, 1.0f, 0.0f };
            continue;
        }

        // Move only presets that have velocity.
        p.transform.translate.x += p.velocity.x * deltaTime;
        p.transform.translate.y += p.velocity.y * deltaTime;
        p.transform.translate.z += p.velocity.z * deltaTime;

        // Make the plane face the camera.
        Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
        Matrix4x4 billboard = camera_->GetBillboardMatrix();

        Matrix4x4 worldNoBillboard =
            MakeAffineMatrix(
                p.transform.scale,
                p.transform.rotate,
                p.transform.translate);

        Matrix4x4 world = Multiply(worldNoBillboard, backToFrontMatrix);
        world = Multiply(world, billboard);

        Matrix4x4 viewProjection = camera_->GetViewProjectionMatrix();
        Matrix4x4 wvp = Multiply(world, viewProjection);

        // Send instance data to the GPU.
        instancingData_[i].World = world;
        instancingData_[i].WVP = wvp;

        float t = p.currentTime / p.lifeTime;
        if (t > 1.0f) { t = 1.0f; }

        // Fade out over lifetime.
        float alpha = 1.0f - t;
        instancingData_[i].color = p.color;
        instancingData_[i].color.w = alpha;
    }
}

void ParticleSystem::Draw()
{
    particleCommon_->CommonDrawSetting();
    ID3D12GraphicsCommandList* cmd = dxCommon_->GetCommandList();

    // t0: instancing StructuredBuffer.
    cmd->SetGraphicsRootDescriptorTable(0, instancingSrvHandleGPU_);

    // t1: particle texture.
    TextureManager* texMan = TextureManager::GetInstance();
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle =
        texMan->GetSrvHandleGPU(textureFilePath_);
    cmd->SetGraphicsRootDescriptorTable(1, texHandle);

    // Draw the same model with instancing.
    Model* model = ModelManager::GetInstance()->FindModel(modelFileName_);
    if (model) {
        model->DrawInstanced(kNumInstance);
    }
}

void ParticleSystem::ShowImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Particle Editor");

    // Select preset.
    const char* items[] = { "CircleBurst", "Explosion", "Smoke" };
    int currentIndex = static_cast<int>(currentType_);
    if (ImGui::Combo("Preset", &currentIndex, items, IM_ARRAYSIZE(items))) {
        ApplyPreset(static_cast<ParticleType>(currentIndex));
    }

    // Edit emitter position.
    ImGui::DragFloat3("Emitter Pos", &emitterPosition_.x, 0.1f);

    // Edit emit settings.
    ImGui::Checkbox("Is Emitting", &isEmitting_);
    ImGui::SliderFloat("Emit Interval", &emitInterval_, 0.01f, 1.0f);
    ImGui::SliderInt("Emit Count", &emitCount_, 1, 10);

    // Edit motion and lifetime.
    ImGui::SliderFloat("Velocity Range", &emitterParam_.velocityRange, 0.0f, 10.0f);

    ImGui::SliderFloat("Life Time Min", &emitterParam_.lifeTimeMin, 0.1f, 5.0f);
    if (emitterParam_.lifeTimeMin > emitterParam_.lifeTimeMax) {
        emitterParam_.lifeTimeMax = emitterParam_.lifeTimeMin;
    }

    ImGui::SliderFloat("Life Time Max", &emitterParam_.lifeTimeMax, 0.1f, 5.0f);
    if (emitterParam_.lifeTimeMax < emitterParam_.lifeTimeMin) {
        emitterParam_.lifeTimeMin = emitterParam_.lifeTimeMax;
    }

    // Edit color.
    ImGui::Checkbox("Random Color", &emitterParam_.randomColor);
    ImGui::ColorEdit4("Base Color", &emitterParam_.baseColor.x);

    ImGui::End();
#endif
}
