#include "Particle.h"

#include "../base/DirectX/DirectXCommon.h"
#include "ParticleCommon.h"
#include "../2d/texture/TextureManager.h"
#include "../3d/model/ModelManager.h"
#include "../../game/camera/Camera.h"
#include "../math/Math.h"
#include <numbers>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"


using Microsoft::WRL::ComPtr;

// =============================
// プリセット一覧
// =============================
static const ParticlePreset kParticlePresets[] = {
    {
        ParticleType::CircleBurst,
        "CircleBurst",
        "Resources/circle.png",
        "plane.obj",
        { 1.0f, 2.0f, 0.5f, 1.5f, {1,1,1,1}, true }
    },
    {
        ParticleType::Explosion,
        "Explosion",
        "Resources/circle.png",
        "plane.obj",
        { 2.0f, 4.0f, 0.3f, 0.8f, {1,0.5f,0.2f,1}, true }
    },
    {
        ParticleType::Smoke,
        "Smoke",
        "Resources/circle.png",
        "plane.obj",
        { 1.5f, 0.5f, 1.0f, 3.0f, {0.4f,0.4f,0.4f,1}, false }
    },
};

static const ParticlePreset& GetPreset(ParticleType type)
{
    for (const auto& p : kParticlePresets) {
        if (p.type == type) { return p; }
    }
    // 見つからなかったら 0 番
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

    // プリセットを適用（テクスチャ・モデル・パラメータをセット）
    ApplyPreset(type);

    // ========= Instancing バッファ作成 =========
    ID3D12Device* device = dxCommon_->GetDevice();
    
    instancingResource_ =
        dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumInstance);

    instancingResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&instancingData_));

    for (uint32_t i = 0; i < kNumInstance; ++i) {
        instancingData_[i].WVP = MakeIdentity4x4();
        instancingData_[i].World = MakeIdentity4x4();
        instancingData_[i].color = Vector4(1, 1, 1, 1);
    }

    // SRV 作成
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

    // ========= 初期パーティクル生成 =========
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        particles_[i] = MakeNewParticle();
    }
}

void ParticleSystem::ApplyPreset(ParticleType type)
{
    currentType_ = type;
    const ParticlePreset& preset = GetPreset(type);

    emitterParam_ = preset.param;
    modelFileName_ = preset.modelName;

    // テクスチャ設定
    TextureManager* texMan = TextureManager::GetInstance();
    texMan->LoadTexture(preset.texturePath);
    textureFilePath_ = preset.texturePath;

    // 既存パーティクルがあれば作り直す
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        particles_[i] = MakeNewParticle();
    }
}

ParticleData ParticleSystem::MakeNewParticle()
{
    // -------------------------
    // 乱数分布の準備
    // -------------------------
    std::uniform_real_distribution<float> distPos(
        -emitterParam_.positionRange, emitterParam_.positionRange);

    std::uniform_real_distribution<float> distVel(
        -emitterParam_.velocityRange, emitterParam_.velocityRange);

    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(
        emitterParam_.lifeTimeMin, emitterParam_.lifeTimeMax);

    ParticleData p{};

    // -------------------------
    // Transform 初期化
    // -------------------------
    p.transform.scale = { 1.0f, 1.0f, 1.0f };
    p.transform.rotate = { 0.0f, 0.0f, 0.0f };

    // エミッタ基準位置 + ランダム
    p.transform.translate = emitterPosition_;
    p.transform.translate.x += distPos(randomEngine_);
    p.transform.translate.y += distPos(randomEngine_);
    p.transform.translate.z += distPos(randomEngine_);

    // -------------------------
    // 種類別の速度
    // -------------------------
    switch (currentType_) {

        // ★ 全方向ランダムに広がる
    case ParticleType::CircleBurst:
        p.velocity = {
            distVel(randomEngine_),
            distVel(randomEngine_),
            distVel(randomEngine_)
        };
        break;

        // ★ 爆発（Explosion）
        // 強めに外側へ吹き飛ばす（CircleBurst より速い）
    case ParticleType::Explosion:
        p.velocity = {
            distVel(randomEngine_) * 2.0f,
            distVel(randomEngine_) * 2.0f,
            distVel(randomEngine_) * 2.0f
        };
        break;

        // ★ Smoke（煙）
        // ゆっくり上昇 + 横は弱くランダム
    case ParticleType::Smoke:
        p.velocity = {
            distVel(randomEngine_) * 0.2f,
            std::fabs(distVel(randomEngine_)) * 0.5f, // 上昇のみ
            distVel(randomEngine_) * 0.2f
        };
        break;
    }

    // -------------------------
    // 色
    // -------------------------
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

    // -------------------------
    // 寿命
    // -------------------------
    p.lifeTime = distTime(randomEngine_);
    p.currentTime = 0.0f;

    return p;
}

void ParticleSystem::Update(float deltaTime)
{
    if (!camera_) { return; }

    const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

    // ===== 追加：放出（出しながら移動）=====
    if (isEmitting_) {
        emitAccumulator_ += emitRate_ * deltaTime;

        // 1.0 たまるごとに 1個生成
        while (emitAccumulator_ >= 1.0f) {
            particles_[emitCursor_] = MakeNewParticle(); // emitterPosition_基準で発生
            emitCursor_ = (emitCursor_ + 1) % kNumInstance;
            emitAccumulator_ -= 1.0f;
        }
    }


    for (uint32_t i = 0; i < kNumInstance; ++i) {
        ParticleData& p = particles_[i];

        p.currentTime += deltaTime;
        if (p.currentTime > p.lifeTime) {
            p = MakeNewParticle();
        }

        // ===== 風（CircleBurst のみ）=====
        if (windEnabled_ && currentType_ == ParticleType::CircleBurst) {
            // 風ベクトル（向き * 強さ）
            Vector3 wind = {
                windDirection_.x * windStrength_,
                windDirection_.y * windStrength_,
                windDirection_.z * windStrength_
            };

            if (windAsAcceleration_) {
                // 加速度として効かせる（だんだん流される）
                p.velocity.x += wind.x * deltaTime;
                p.velocity.y += wind.y * deltaTime;
                p.velocity.z += wind.z * deltaTime;
            } else {
                // 速度に直足し（一定で流す）
                p.velocity.x += wind.x;
                p.velocity.y += wind.y;
                p.velocity.z += wind.z;
            }
        }


        p.transform.translate.x += p.velocity.x * deltaTime;
        p.transform.translate.y += p.velocity.y * deltaTime;
        p.transform.translate.z += p.velocity.z * deltaTime;



        // ===== world行列 =====

 // 板ポリの表⾯向き補正（Yを180°）
        Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);

        // カメラ側をビルボード行列
        Matrix4x4 billboard = camera_->GetBillboardMatrix();

        // 通常の SRT
        Matrix4x4 worldNoBillboard =
            MakeAffineMatrix(
                p.transform.scale,
                p.transform.rotate,
                p.transform.translate);

        // 補正を適用した world
        Matrix4x4 world = Multiply(worldNoBillboard, backToFrontMatrix);
        world = Multiply(world, billboard);

        // ここで wvp を作ってあげる ★ 追加
        Matrix4x4 viewProjection = camera_->GetViewProjectionMatrix();
        Matrix4x4 wvp = Multiply(world, viewProjection);

        // GPU へ書き込み
        instancingData_[i].World = world;
        instancingData_[i].WVP = wvp;

        float t = p.currentTime / p.lifeTime;
        if (t > 1.0f) { t = 1.0f; }

        float alpha = 1.0f - t;

        instancingData_[i].color = p.color;
        instancingData_[i].color.w = alpha;
    }
}

void ParticleSystem::Draw()
{
    particleCommon_->CommonDrawSetting();
    ID3D12GraphicsCommandList* cmd = dxCommon_->GetCommandList();

    // t0 = StructuredBuffer
    cmd->SetGraphicsRootDescriptorTable(0, instancingSrvHandleGPU_);

    // t1 = テクスチャ
    TextureManager* texMan = TextureManager::GetInstance();
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle =
        texMan->GetSrvHandleGPU(textureFilePath_);
    cmd->SetGraphicsRootDescriptorTable(1, texHandle);

    // モデルをインスタンス描画
    Model* model = ModelManager::GetInstance()->FindModel(modelFileName_);
    if (model) {
        model->DrawInstanced(kNumInstance);
    }
}

void ParticleSystem::ShowImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Particle Editor");

    // プリセット選択
    const char* items[] = { "CircleBurst", "Explosion", "Smoke" };
    int currentIndex = static_cast<int>(currentType_);
    if (ImGui::Combo("Preset", &currentIndex, items, IM_ARRAYSIZE(items))) {
        ApplyPreset(static_cast<ParticleType>(currentIndex));
    }

    // エミッタ位置
    ImGui::DragFloat3("Emitter Pos", &emitterPosition_.x, 0.1f);

    // 細かいパラメータもそのまま編集可能
    ImGui::SliderFloat("Position Range", &emitterParam_.positionRange, 0.0f, 10.0f);
    ImGui::SliderFloat("Velocity Range", &emitterParam_.velocityRange, 0.0f, 10.0f);

    ImGui::SliderFloat("Life Time Min", &emitterParam_.lifeTimeMin, 0.1f, 5.0f);
    if (emitterParam_.lifeTimeMin > emitterParam_.lifeTimeMax) {
        emitterParam_.lifeTimeMax = emitterParam_.lifeTimeMin;
    }

    ImGui::SliderFloat("Life Time Max", &emitterParam_.lifeTimeMax, 0.1f, 5.0f);
    if (emitterParam_.lifeTimeMax < emitterParam_.lifeTimeMin) {
        emitterParam_.lifeTimeMin = emitterParam_.lifeTimeMax;
    }

    ImGui::Checkbox("Random Color", &emitterParam_.randomColor);
    ImGui::ColorEdit4("Base Color", &emitterParam_.baseColor.x);
    ImGui::Checkbox("Wind Enabled", &windEnabled_);



    ImGui::End();
#endif
   
}
