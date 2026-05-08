#include "Object3d.h"
#include "Object3dCommon.h"
#include "../model/Model.h"
#include "../model/ModelManager.h"

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    object3dCommon_ = object3dCommon;

    camera_ = object3dCommon_->GetDefaultCamera();

    transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.3f, 0.0f, 0.0f}, {0.0f, 3.0f, -10.0f} };

    InitializeTransformationMatrix();
    InitializeDirectionalLight();
    InitializeCameraForGPU();
    InitializeMaterial();
    SetEnvironmentTexture(environmentTextureFilePath_);
    InitializeSkinningInformation();

}

void Object3d::Update()
{
    assert(transformationMatrixData_);

    Quaternion animationRotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool hasAnimationRotate = false;

    if (isAnimationPlaying_ && animation_.duration > 0.0f) {
        // ひとまず 60fps 前提で時刻を進める
        animationTime_ += 1.0f / 60.0f;

        // 最後まで行ったら先頭に戻してループ再生する
        animationTime_ = std::fmod(animationTime_, animation_.duration);
    }

    if (hasSkeleton_ && isAnimationPlaying_ && !animation_.nodeAnimations.empty()) {
        // Skeleton に animation を適用する
        ApplyAnimation(skeleton_, animation_, animationTime_);
    }

    if (!hasSkeleton_ && isAnimationPlaying_ && !animation_.nodeAnimations.empty()) {
        auto it = animation_.nodeAnimations.find(animationNodeName_);

        // 指定した node 名の Animation があるときだけ再生する
        if (it != animation_.nodeAnimations.end()) {
            const NodeAnimation& nodeAnimation = it->second;

            // translate のキーがあれば現在時刻の値を反映する
            if (!nodeAnimation.translate.keyframes.empty()) {
                transform.translate =
                    CalculateValue(nodeAnimation.translate.keyframes, animationTime_);
            }

            // scale のキーがあれば現在時刻の値を反映する
            if (!nodeAnimation.scale.keyframes.empty()) {
                transform.scale =
                    CalculateValue(nodeAnimation.scale.keyframes, animationTime_);
            }

            // rotate のキーがあれば現在時刻の値を取得する
            if (!nodeAnimation.rotate.keyframes.empty()) {
                animationRotate =
                    CalculateValue(nodeAnimation.rotate.keyframes, animationTime_);
                hasAnimationRotate = true;
            }
        }
    }

    if (hasSkeleton_) {
        // animation 適用後の transform から Skeleton 行列を更新する
        UpdateSkeleton(skeleton_);
    }

    if (hasSkinCluster_) {
        // 現在の Skeleton 状態から SkinCluster を更新する
        UpdateSkinCluster(skinCluster_, skeleton_);
        ApplySkinningCompute();

    }


    Matrix4x4 worldMatrix;


    if (hasAnimationRotate) {
        // アニメーション回転があるときは Quaternion 版を使う
        worldMatrix =
            MakeAffineMatrix(transform.scale, animationRotate, transform.translate);
    } else {
        // これまで通り Euler 角版を使う
        worldMatrix =
            MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    }

    Matrix4x4 worldViewProjectionMatrix;

    if (camera_) {
        const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
        viewProjectionMatrix_ = vp;
        worldViewProjectionMatrix = Multiply(worldMatrix, vp);
    } else {
        worldViewProjectionMatrix = worldMatrix;
        viewProjectionMatrix_ = MakeIdentity4x4();
    }

    transformationMatrixData_->WVP = worldViewProjectionMatrix;
    transformationMatrixData_->World = worldMatrix;

    if (cameraData_) {
        cameraData_->worldPosition = camera_ ? camera_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
        cameraData_->padding = 0.0f;
    }
}

void Object3d::Draw()
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    object3dCommon_->CommonDrawSetting();

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        3, cameraResource_->GetGPUVirtualAddress());

    if (model_) {
        // モデルに設定されているテクスチャを t1 に設定する
        commandList->SetGraphicsRootDescriptorTable(
            5,
            TextureManager::GetInstance()->GetSrvHandleGPU(
                model_->GetModelData().material.textureFilePath));
    }

    // 環境マップを t2 に設定する
    commandList->SetGraphicsRootDescriptorTable(
        6,
        TextureManager::GetInstance()->GetSrvHandleGPU(environmentTextureFilePath_));

    if (model_) {
        if (hasSkinCluster_) {
            // Skinning 用の influence VBV も渡して描画する
            model_->DrawWithSkinnedVertexBuffer(skinCluster_.skinnedVertexBufferView);

        } else {
            // 通常描画を行う
            model_->Draw();
        }
    }
}




MaterialData Object3d::LoadMaterialTemplateFile(
    const std::string& directoryPath,
    const std::string& mtlFileName,
    MaterialData& material)
{
    std::ifstream file(directoryPath + "/" + mtlFileName);
    if (!file.is_open()) {
        return material;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream s(line);
        std::string id;
        s >> id;

        if (id == "map_Kd") {
            std::string texName;
            s >> texName;
            material.textureFilePath = directoryPath + "/" + texName;
        }
    }

    return material;
}

void Object3d::InitializeTransformationMatrix()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    transformationMatrixResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));

    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}

void Object3d::InitializeDirectionalLight()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    directionalLightResource_ =
        dxCommon->CreateBufferResource(sizeof(DirectionalLight));

    directionalLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&directionalLightData_));

    directionalLightData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f);
    directionalLightData_->intensity = 0.2f;
}

void Object3d::InitializeCameraForGPU()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    cameraResource_ = dxCommon->CreateBufferResource(sizeof(CameraForGPU));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

    cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
    cameraData_->padding = 0.0f;
}

void Object3d::SetModel(const std::string& filePath)
{
    model_ = ModelManager::GetInstance()->FindModel(filePath);

    hasSkeleton_ = false;
    hasSkinCluster_ = false;

    if (model_) {
        const ModelData& modelData = model_->GetModelData();

        // rootNode に名前や子が入っていれば Skeleton を作る
        if (!modelData.rootNode.name.empty() || !modelData.rootNode.children.empty()) {
            skeleton_ = CreateSkeleton(modelData.rootNode);
            UpdateSkeleton(skeleton_);
            hasSkeleton_ = true;
        }

        // Skeleton と skinClusterData の両方があるなら SkinCluster を作る
        if (hasSkeleton_ && !modelData.skinClusterData.empty()) {
            skinCluster_ = CreateSkinCluster(
                object3dCommon_->GetDxCommon(),
                object3dCommon_->GetSrvManager(),
                skeleton_,
                modelData);

            hasSkinCluster_ = true;
        }
    }
}

void Object3d::SetTexture(const std::string& filePath)
{
    TextureManager* texMan = TextureManager::GetInstance();
    texMan->LoadTexture(filePath);

    if (model_) {
        model_->SetTextureIndex(texMan->GetTextureIndexByFilePath(filePath));
    }
}

void Object3d::InitializeMaterial()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = Vector4(1, 1, 1, 1);
    materialData_->lightingType = static_cast<int>(LightingType::HalfLambert);
    materialData_->environmentCoefficient = 0.0f;
    materialData_->uvTransform = MakeIdentity4x4();
}

void Object3d::SetColor(const Vector4& color)
{
    if (materialData_) {
        materialData_->color = color;
    }
}

void Object3d::SetEnvironmentTexture(const std::string& filePath)
{
    environmentTextureFilePath_ = filePath;
    TextureManager::GetInstance()->LoadTexture(environmentTextureFilePath_);
}

void Object3d::SetEnvironmentCoefficient(float coefficient)
{
    if (materialData_) {
        materialData_->environmentCoefficient = coefficient;
    }
}

void Object3d::DrawInstanced(UINT instanceCount)
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    object3dCommon_->CommonDrawSetting();

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        3, cameraResource_->GetGPUVirtualAddress());

    if (hasSkinCluster_) {
        // Skinning 用の MatrixPalette を t0 に設定する
        commandList->SetGraphicsRootDescriptorTable(
            4,
            skinCluster_.paletteSrvHandle.second);
    }

    if (model_) {
        // モデルに設定されているテクスチャを t1 に設定する
        commandList->SetGraphicsRootDescriptorTable(
            5,
            TextureManager::GetInstance()->GetSrvHandleGPU(
                model_->GetModelData().material.textureFilePath));
    }

    // 環境マップを t2 に設定する
    commandList->SetGraphicsRootDescriptorTable(
        6,
        TextureManager::GetInstance()->GetSrvHandleGPU(environmentTextureFilePath_));

    if (model_) {
        if (hasSkinCluster_) {
            // Skinning 用の influence VBV も渡して描画する
            model_->DrawInstancedWithSkinnedVertexBuffer(
                instanceCount,
                skinCluster_.skinnedVertexBufferView);

        } else {
            // 通常のインスタンシング描画を行う
            model_->DrawInstanced(instanceCount);
        }
    }
}




// ComputeShader に渡す頂点数情報の定数バッファを初期化する
void Object3d::InitializeSkinningInformation()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    skinningInformationResource_ =
        dxCommon->CreateBufferResource(sizeof(SkinningInformationForGPU));

    skinningInformationResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&skinningInformationData_));

    skinningInformationData_->numVertices = 0;
    skinningInformationData_->padding[0] = 0;
    skinningInformationData_->padding[1] = 0;
    skinningInformationData_->padding[2] = 0;
}

// ComputeShader でスキニングを実行する
void Object3d::ApplySkinningCompute()
{
    if (!hasSkinCluster_ || !model_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList =
        object3dCommon_->GetDxCommon()->GetCommandList();

    const ModelData& modelData = model_->GetModelData();
    uint32_t vertexCount = GetSkinClusterVertexCount(modelData);

    // ComputeShader に渡す頂点数を更新する
    skinningInformationData_->numVertices = vertexCount;

    // ComputeShader 用の RootSignature と PSO を設定する
    object3dCommon_->SkinningComputeSetting();

    // UAV へ書き込む前に Compute 用状態へ遷移する
    D3D12_RESOURCE_BARRIER barrierToUav{};
    barrierToUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToUav.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToUav.Transition.pResource = skinCluster_.skinnedVertexResource.Get();
    barrierToUav.Transition.StateBefore = skinCluster_.skinnedVertexCurrentState;
    barrierToUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToUav);

    // 現在の ResourceState を更新する
    skinCluster_.skinnedVertexCurrentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;


    // t0 : MatrixPalette
    commandList->SetComputeRootDescriptorTable(
        0,
        skinCluster_.paletteSrvHandle.second);

    // t1 : InputVertices
    commandList->SetComputeRootDescriptorTable(
        1,
        model_->GetCombinedVertexSrvHandleGPU());


    // t2 : Influences
    commandList->SetComputeRootDescriptorTable(
        2,
        skinCluster_.influenceSrvHandle.second);


    // u0 : OutputVertices
    commandList->SetComputeRootDescriptorTable(
        3,
        skinCluster_.skinnedVertexUavHandle.second);

    // b0 : SkinningInformation
    commandList->SetComputeRootConstantBufferView(
        4,
        skinningInformationResource_->GetGPUVirtualAddress());

    // 1024 thread ごとに切り上げて Dispatch する
    commandList->Dispatch((vertexCount + 255) / 256, 1, 1);


    // UAV 書き込み完了を保証する
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = skinCluster_.skinnedVertexResource.Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    // 描画で読める状態へ戻す
    D3D12_RESOURCE_BARRIER barrierToVertexAndConstantBuffer{};
    barrierToVertexAndConstantBuffer.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToVertexAndConstantBuffer.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToVertexAndConstantBuffer.Transition.pResource = skinCluster_.skinnedVertexResource.Get();
    barrierToVertexAndConstantBuffer.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToVertexAndConstantBuffer.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrierToVertexAndConstantBuffer.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToVertexAndConstantBuffer);

    // 描画用の ResourceState へ戻したことを記録する
    skinCluster_.skinnedVertexCurrentState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

}
