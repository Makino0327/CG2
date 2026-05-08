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
// パーティクルのプリセット一覧
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

    // 見つからない場合は先頭のプリセットを使う
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

    // プリセットからテクスチャや形状設定を反映する
    ApplyPreset(type);

    // インスタンシング用のGPUバッファを作る
    ID3D12Device* device = dxCommon_->GetDevice();
    instancingResource_ =
        dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumInstance);

    InitializeGPUParticleResource();
    InitializePerViewResource();

    // GPU発生用のEmitter情報を作る
    InitializeEmitterResource();

    // GPU発生用のPerFrame情報を作る
    InitializePerFrameResourceForCS();

    // GPU発生用の空きCounterを作る
    InitializeFreeCounterResource();

    // Particle本体とCounterを初期化する
    InitializeParticleCS();


    // Emitterの初期値を設定する
    emitterSphere_.translate = { 0.0f, 0.0f, 0.0f };
    emitterSphere_.radius = 1.0f;
    emitterSphere_.count = 10;
    emitterSphere_.frequency = 0.5f;
    emitterSphere_.frequencyTime = 0.0f;
    emitterSphere_.emit = 0;

    // PerFrameの初期値を設定する
    perFrameForCS_.time = 0.0f;
    perFrameForCS_.deltaTime = 0.0f;


    instancingResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&instancingData_));

    for (uint32_t i = 0; i < kNumInstance; ++i) {
        instancingData_[i].WVP = MakeIdentity4x4();
        instancingData_[i].World = MakeIdentity4x4();
        instancingData_[i].color = Vector4(1, 1, 1, 0);
    }

    // インスタンシングバッファをSRVとして登録する
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

    // 最初は非表示状態のパーティクルで初期化する
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        particles_[i] = MakeDeadParticle();
    }

    // // 現在の形状設定に応じて描画オブジェクトを作る
    if (meshType_ == EffectMeshType::Ring) {
        ring_ = std::make_unique<Ring>();
        ring_->Initialize(dxCommon_);
        cylinder_.reset();
    } else if (meshType_ == EffectMeshType::Cylinder) {
        cylinder_ = std::make_unique<Cylinder>();
        cylinder_->Initialize(dxCommon_);
        ring_.reset();
    } else {
        ring_.reset();
        cylinder_.reset();
    }

}

void ParticleSystem::ApplyPreset(ParticleType type)
{
    currentType_ = type;
    const ParticlePreset& preset = GetPreset(type);

    emitterParam_ = preset.param;
    modelFileName_ = preset.modelName;

    if (meshType_ == EffectMeshType::Ring) {
        textureFilePath_ = "Resources/gradationLine.png";
    } else {
        textureFilePath_ = "Resources/circle2.png";
    }

    TextureManager* texMan = TextureManager::GetInstance();

    // Ring のときは専用のグラデーションテクスチャを使う
    if (meshType_ == EffectMeshType::Ring) {
        textureFilePath_ = "Resources/gradationLine.png";
    } else {
        textureFilePath_ = preset.texturePath;
    }

    // 使用するテクスチャを読み込む
    texMan->LoadTexture(textureFilePath_);

    // プリセット切り替え時に古いパーティクルを消す
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        particles_[i] = MakeDeadParticle();
    }

    if (dxCommon_) {
        if (meshType_ == EffectMeshType::Ring) {
            ring_ = std::make_unique<Ring>();
            ring_->Initialize(dxCommon_);
            cylinder_.reset();
        } else if (meshType_ == EffectMeshType::Cylinder) {
            cylinder_ = std::make_unique<Cylinder>();
            cylinder_->Initialize(dxCommon_);
            ring_.reset();
        } else {
            ring_.reset();
            cylinder_.reset();
        }
    }

}

ParticleData ParticleSystem::MakeNewParticle()
{
    // ランダム値の分布を用意する
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

    // 丸ではなく細長い板ポリ形状にしている
    float scale = distScale(randomEngine_);
    p.transform.scale = { scale * 0.15f, scale * 1.8f, scale };

    // 同じ位置に重なって見えるように回転だけランダムにする
    p.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine_) };

    // 発生位置は常にエミッター位置にする
    p.transform.translate = emitterPosition_;

    switch (currentType_) {
    case ParticleType::CircleBurst:
        // CircleBurst はその場に留める
        p.velocity = { 0.0f, 0.0f, 0.0f };
        break;

    case ParticleType::Explosion:
        // Explosion は全方向へ強く飛ばす
        p.velocity = {
            distVel(randomEngine_) * 2.0f,
            distVel(randomEngine_) * 2.0f,
            distVel(randomEngine_) * 2.0f
        };
        break;

    case ParticleType::Smoke:
        // Smoke はゆっくり上方向へ流す
        p.velocity = {
            distVel(randomEngine_) * 0.2f,
            std::fabs(distVel(randomEngine_)) * 0.5f,
            distVel(randomEngine_) * 0.2f
        };
        break;
    }

    // 色を決める
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

    // 生存時間を設定する
    p.lifeTime = distTime(randomEngine_);
    p.currentTime = 0.0f;
    p.isAlive = true;

    return p;
}

ParticleData ParticleSystem::MakeDeadParticle()
{
    ParticleData p{};

    // DrawInstanced は固定数描画なので未使用個体は見えなくしておく
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

    // VertexShader で使うビュー情報を更新する
    perViewData_->viewProjection = camera_->GetViewProjectionMatrix();
    perViewData_->billboardMatrix = camera_->GetBillboardMatrix();


    // // Cylinder は動かさず、その場に1本だけ置く
    if (meshType_ == EffectMeshType::Cylinder) {
        for (uint32_t i = 0; i < kNumInstance; ++i) {
            instancingData_[i].World = MakeIdentity4x4();
            instancingData_[i].WVP = MakeIdentity4x4();
            instancingData_[i].color = { 1.0f, 1.0f, 1.0f, 0.0f };
        }

        // // Emitting が OFF なら表示しない
        if (!isEmitting_) {
            return;
        }

        Transform transform{};
        transform.scale = { 1.0f, 1.0f, 1.0f };
        transform.rotate = { 0.0f, 0.0f, 0.0f };
        transform.translate = emitterPosition_;

        Matrix4x4 world = MakeAffineMatrix(
            transform.scale,
            transform.rotate,
            transform.translate);

        Matrix4x4 viewProjection = camera_->GetViewProjectionMatrix();
        Matrix4x4 wvp = Multiply(world, viewProjection);

        // // 1本目だけ表示する
        instancingData_[0].World = world;
        instancingData_[0].WVP = wvp;
        instancingData_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };

        return;
    }


    // ImGuiで動かした位置をEmitterにも反映する
    emitterSphere_.translate = emitterPosition_;

    // GPUへ送る時間情報を更新する
    perFrameForCS_.time += deltaTime;
    perFrameForCS_.deltaTime = deltaTime;

    // Emitterの経過時間を進める
    emitterSphere_.frequencyTime += deltaTime;

    // 指定時間を超えたら今フレーム発生する
    if (emitterSphere_.frequency <= emitterSphere_.frequencyTime) {
        emitterSphere_.frequencyTime -= emitterSphere_.frequency;
        emitterSphere_.emit = 1;
    } else {
        emitterSphere_.emit = 0;
    }

    // EmittingがOFFなら発生フラグを落とす
    if (!isEmitting_) {
        emitterSphere_.emit = 0;
    }

    // CPU側の最新値をConstantBufferへ反映する
    *emitterData_ = emitterSphere_;
    *perFrameDataForCS_ = perFrameForCS_;

    // 毎フレームGPUでParticleを発生させる
    DispatchEmitParticleCS();

    // 毎フレームGPUでParticleを移動・寿命更新する
    DispatchUpdateParticleCS();



    //for (uint32_t i = 0; i < kNumInstance; ++i) {
    //    ParticleData& p = particles_[i];

    //    // 非アクティブなものは透明のままにする
    //    if (!p.isAlive) {
    //        instancingData_[i].World = MakeIdentity4x4();
    //        instancingData_[i].WVP = MakeIdentity4x4();
    //        instancingData_[i].color = { 1.0f, 1.0f, 1.0f, 0.0f };
    //        continue;
    //    }

    //    // 寿命切れのものは非表示へ戻す
    //    p.currentTime += deltaTime;
    //    if (p.currentTime > p.lifeTime) {
    //        p = MakeDeadParticle();
    //        instancingData_[i].World = MakeIdentity4x4();
    //        instancingData_[i].WVP = MakeIdentity4x4();
    //        instancingData_[i].color = { 1.0f, 1.0f, 1.0f, 0.0f };
    //        continue;
    //    }

    //    // 速度を持つプリセットだけ移動させる
    //    p.transform.translate.x += p.velocity.x * deltaTime;
    //    p.transform.translate.y += p.velocity.y * deltaTime;
    //    p.transform.translate.z += p.velocity.z * deltaTime;

    //    // 板ポリをカメラ正面へ向ける
    //    Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
    //    Matrix4x4 billboard = camera_->GetBillboardMatrix();

    //    Matrix4x4 worldNoBillboard =
    //        MakeAffineMatrix(
    //            p.transform.scale,
    //            p.transform.rotate,
    //            p.transform.translate);

    //    Matrix4x4 world = Multiply(worldNoBillboard, backToFrontMatrix);
    //    world = Multiply(world, billboard);

    //    Matrix4x4 viewProjection = camera_->GetViewProjectionMatrix();
    //    Matrix4x4 wvp = Multiply(world, viewProjection);

    //    // GPU へインスタンス情報を書き込む
    //    instancingData_[i].World = world;
    //    instancingData_[i].WVP = wvp;

    //    float t = p.currentTime / p.lifeTime;
    //    if (t > 1.0f) { t = 1.0f; }

    //    // 生存時間に応じてフェードアウトさせる
    //    float alpha = 1.0f - t;
    //    instancingData_[i].color = p.color;
    //    instancingData_[i].color.w = alpha;
    //}
}

void ParticleSystem::Draw()
{
    particleCommon_->CommonDrawSetting();
    ID3D12GraphicsCommandList* cmd = dxCommon_->GetCommandList();

    // t0: インスタンシング用 StructuredBuffer
       // t0 : GPU Particle 配列
    cmd->SetGraphicsRootDescriptorTable(0, particleSrvHandleGPU_);


    // t1: パーティクル用テクスチャ
    TextureManager* texMan = TextureManager::GetInstance();
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle =
        texMan->GetSrvHandleGPU(textureFilePath_);
    // t1 : パーティクル用テクスチャ
    cmd->SetGraphicsRootDescriptorTable(1, texHandle);

    // b0 : VertexShader 用の PerView
    cmd->SetGraphicsRootConstantBufferView(
        2,
        perViewResource_->GetGPUVirtualAddress());


    // 形状の種類に応じて描画方法を切り替える
    if (meshType_ == EffectMeshType::Plane) {
        Model* model = ModelManager::GetInstance()->FindModel(modelFileName_);
        if (model) {
            model->DrawInstanced(kNumInstance);
        }
    } else if (meshType_ == EffectMeshType::Ring) {
        if (ring_) {
            ring_->DrawInstanced(kNumInstance);
        }
    }
    else if (meshType_ == EffectMeshType::Cylinder) {
        if (cylinder_) {
            cylinder_->DrawInstanced(kNumInstance);
        }
    }

}

void ParticleSystem::ShowImGui(const char* windowName)
{
#ifdef USE_IMGUI
    ImGui::Begin(windowName);

    const char* meshItems[] = { "Plane", "Ring", "Cylinder" };

    int currentMesh = static_cast<int>(meshType_);
    if (ImGui::Combo("Mesh Type", &currentMesh, meshItems, IM_ARRAYSIZE(meshItems))) {
        // // ImGui で選んだ形状に切り替える
        SetMeshType(static_cast<EffectMeshType>(currentMesh));
    }

    // // エミッター位置を調整する
    ImGui::DragFloat3("Emitter Pos", &emitterPosition_.x, 0.1f);
    // GPU発生用Emitterの半径を調整する
    ImGui::SliderFloat("Emitter Radius", &emitterSphere_.radius, 0.0f, 5.0f);

    // // 放出設定を調整する
    ImGui::Checkbox("Is Emitting", &isEmitting_);
    ImGui::SliderFloat("Emit Interval", &emitterSphere_.frequency, 0.01f, 1.0f);
    ImGui::SliderInt("Emit Count", reinterpret_cast<int*>(&emitterSphere_.count), 1, 10);


    // // 速度を調整する
    ImGui::SliderFloat("Velocity Range", &emitterParam_.velocityRange, 0.0f, 10.0f);

    ImGui::SliderFloat("Life Time Min", &emitterParam_.lifeTimeMin, 0.1f, 5.0f);
    if (emitterParam_.lifeTimeMin > emitterParam_.lifeTimeMax) {
        emitterParam_.lifeTimeMax = emitterParam_.lifeTimeMin;
    }

    ImGui::SliderFloat("Life Time Max", &emitterParam_.lifeTimeMax, 0.1f, 5.0f);
    if (emitterParam_.lifeTimeMax < emitterParam_.lifeTimeMin) {
        emitterParam_.lifeTimeMin = emitterParam_.lifeTimeMax;
    }

    // // 色を調整する
    ImGui::Checkbox("Random Color", &emitterParam_.randomColor);
    ImGui::ColorEdit4("Base Color", &emitterParam_.baseColor.x);

    ImGui::End();
#endif
}


void ParticleSystem::SetMeshType(EffectMeshType type)
{
    // // 新しい形状を保存する
    meshType_ = type;

    // // 使用するテクスチャを形状に合わせて切り替える
    if (meshType_ == EffectMeshType::Ring || meshType_ == EffectMeshType::Cylinder) {
        textureFilePath_ = "Resources/gradationLine.png";
    } else {
        textureFilePath_ = "Resources/circle2.png";
    }

    // // まだ初期化前ならここで終わる
    if (!dxCommon_) {
        return;
    }

    // // 形状に応じて描画オブジェクトを作り直す
    if (meshType_ == EffectMeshType::Ring) {
        ring_ = std::make_unique<Ring>();
        ring_->Initialize(dxCommon_);
        cylinder_.reset();
    } else if (meshType_ == EffectMeshType::Cylinder) {
        cylinder_ = std::make_unique<Cylinder>();
        cylinder_->Initialize(dxCommon_);
        ring_.reset();
    } else {
        ring_.reset();
        cylinder_.reset();
    }

    // // 形状を変えたときに以前の粒子を消しておく
    for (uint32_t i = 0; i < kNumInstance; ++i) {
        particles_[i] = MakeDeadParticle();
    }
}

// GPU Particle 用 Resource と View を作る
void ParticleSystem::InitializeGPUParticleResource()
{
    // GPU が自由に読み書きできる Particle 配列を作る
    particleResource_ =
        dxCommon_->CreateUAVBufferResource(sizeof(ParticleCS) * kNumInstance);

    // SRV と UAV の index を確保する
    particleSrvIndex_ = srvManager_->Allocate();
    particleUavIndex_ = srvManager_->Allocate();

    // GPU ハンドルを保存しておく
    particleSrvHandleGPU_ =
        srvManager_->GetGPUDescriptorHandle(particleSrvIndex_);
    particleUavHandleGPU_ =
        srvManager_->GetGPUDescriptorHandle(particleUavIndex_);

    // VertexShader から読むための SRV を作る
    srvManager_->CreateSRVforStructuredBuffer(
        particleSrvIndex_,
        particleResource_.Get(),
        kNumInstance,
        sizeof(ParticleCS));

    // ComputeShader から書くための UAV を作る
    srvManager_->CreateUAVforStructuredBuffer(
        particleUavIndex_,
        particleResource_.Get(),
        kNumInstance,
        sizeof(ParticleCS));
}

// VertexShader 用の PerView 定数バッファを作る
void ParticleSystem::InitializePerViewResource()
{
    perViewResource_ =
        dxCommon_->CreateBufferResource(sizeof(ParticlePerView));

    perViewResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&perViewData_));

    perViewData_->viewProjection = MakeIdentity4x4();
    perViewData_->billboardMatrix = MakeIdentity4x4();
}

// ComputeShader で Particle を初期化する
void ParticleSystem::InitializeParticleCS()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // ComputeShader 用の RootSignature と PSO を設定する
    particleCommon_->InitializeParticleComputeSetting();

    // Particle Resource を UAV 書き込み用状態へ遷移する
    D3D12_RESOURCE_BARRIER barrierToUav{};
    barrierToUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToUav.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToUav.Transition.pResource = particleResource_.Get();
    barrierToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrierToUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToUav);

    // u0 : Particle 配列
        // u0 : Particleデータ
    commandList->SetComputeRootDescriptorTable(0, particleUavHandleGPU_);

    // u1 : 空きCounter
    commandList->SetComputeRootDescriptorTable(1, freeCounterUavHandleGPU_);


    // 10 個の Particle を初期化するので 1 グループだけ起動する
    commandList->Dispatch((kNumInstance + 255) / 256, 1, 1);


    // UAV 書き込み完了を保証する
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = particleResource_.Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    // VertexShader から読める状態へ戻す
    D3D12_RESOURCE_BARRIER barrierToSrv{};
    barrierToSrv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToSrv.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToSrv.Transition.pResource = particleResource_.Get();
    barrierToSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    barrierToSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToSrv);
}

// GPU発生用EmitterのConstantBufferを作る
void ParticleSystem::InitializeEmitterResource()
{
    emitterResource_ = dxCommon_->CreateBufferResource(sizeof(EmitterSphere));

    emitterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&emitterData_));

    // 初期値を入れておく
    *emitterData_ = emitterSphere_;
}

// GPU発生用PerFrameのConstantBufferを作る
void ParticleSystem::InitializePerFrameResourceForCS()
{
    perFrameResourceForCS_ = dxCommon_->CreateBufferResource(sizeof(PerFrame));

    perFrameResourceForCS_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&perFrameDataForCS_));

    // 初期値を入れておく
    *perFrameDataForCS_ = perFrameForCS_;
}

// GPU発生用CounterのUAVを作る
void ParticleSystem::InitializeFreeCounterResource()
{
    // int32_tを1個だけ持つUAVバッファを作る
    freeCounterResource_ = dxCommon_->CreateUAVBufferResource(sizeof(int32_t));

    // Counter用のUAV indexを確保する
    freeCounterUavIndex_ = srvManager_->Allocate();

    // GPUハンドルを保存する
    freeCounterUavHandleGPU_ =
        srvManager_->GetGPUDescriptorHandle(freeCounterUavIndex_);

    // 要素数1、strideがint32_tのUAVを作る
    srvManager_->CreateUAVforStructuredBuffer(
        freeCounterUavIndex_,
        freeCounterResource_.Get(),
        1,
        sizeof(int32_t));
}

// 毎フレームGPUでParticleを発生させる
void ParticleSystem::DispatchEmitParticleCS()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // 発生用ComputeShaderを使う設定にする
    particleCommon_->InitializeEmitParticleComputeSetting();

    // Particle ResourceをUAVとして使える状態へ遷移する
    D3D12_RESOURCE_BARRIER barrierToUav{};
    barrierToUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToUav.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToUav.Transition.pResource = particleResource_.Get();
    barrierToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    barrierToUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToUav);

    // b0 : Emitter
    commandList->SetComputeRootConstantBufferView(
        0,
        emitterResource_->GetGPUVirtualAddress());

    // b1 : PerFrame
    commandList->SetComputeRootConstantBufferView(
        1,
        perFrameResourceForCS_->GetGPUVirtualAddress());

    // u0 : Particle
    commandList->SetComputeRootDescriptorTable(2, particleUavHandleGPU_);

    // u1 : Counter
    commandList->SetComputeRootDescriptorTable(3, freeCounterUavHandleGPU_);

    // Emitterは1個だけなので1thread groupでよい
    commandList->Dispatch(1, 1, 1);

    // UAVの書き込み完了を保証する
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = particleResource_.Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    // 描画でSRVとして読むため戻す
    D3D12_RESOURCE_BARRIER barrierToSrv{};
    barrierToSrv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToSrv.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToSrv.Transition.pResource = particleResource_.Get();
    barrierToSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    barrierToSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToSrv);
}

// 毎フレームGPUでParticleを更新する
void ParticleSystem::DispatchUpdateParticleCS()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // 更新用ComputeShaderを使う設定にする
    particleCommon_->InitializeUpdateParticleComputeSetting();

    // Particle ResourceをUAVとして使える状態へ遷移する
    D3D12_RESOURCE_BARRIER barrierToUav{};
    barrierToUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToUav.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToUav.Transition.pResource = particleResource_.Get();
    barrierToUav.Transition.StateBefore = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    barrierToUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToUav);

    // b0 : PerFrame
    commandList->SetComputeRootConstantBufferView(
        0,
        perFrameResourceForCS_->GetGPUVirtualAddress());

    // u0 : Particle
    commandList->SetComputeRootDescriptorTable(1, particleUavHandleGPU_);

    // 1024個を256threadずつで更新する
    commandList->Dispatch((kNumInstance + 255) / 256, 1, 1);

    // UAVの書き込み完了を保証する
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = particleResource_.Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    // 描画でSRVとして読むため戻す
    D3D12_RESOURCE_BARRIER barrierToSrv{};
    barrierToSrv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToSrv.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToSrv.Transition.pResource = particleResource_.Get();
    barrierToSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    barrierToSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToSrv);
}
