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

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        3, cameraResource_->GetGPUVirtualAddress());

    if (model_) {
        // モデルに設定されているテクスチャを t0 に設定する
        commandList->SetGraphicsRootDescriptorTable(
            4,
            TextureManager::GetInstance()->GetSrvHandleGPU(
                model_->GetModelData().material.textureFilePath));
    }

    // 環境マップを t1 に設定する
    commandList->SetGraphicsRootDescriptorTable(
        5, TextureManager::GetInstance()->GetSrvHandleGPU(environmentTextureFilePath_));

    if (model_) {
        model_->Draw();
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
    model_ = ModelManager::GetInstance()->FindModel(filePath); // // モデルを取得する

    hasSkeleton_ = false; // // いったん Skeleton 無しに戻す

    if (model_) {
        const ModelData& modelData = model_->GetModelData(); // // モデルデータを参照する

        // // rootNode に名前や子が入っていれば Skeleton を作る
        if (!modelData.rootNode.name.empty() || !modelData.rootNode.children.empty()) {
            skeleton_ = CreateSkeleton(modelData.rootNode); // // Node 階層から Skeleton を生成する
            UpdateSkeleton(skeleton_);                      // // 初期姿勢の行列を計算する
            hasSkeleton_ = true;                            // // Skeleton を持つ状態にする
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

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        3, cameraResource_->GetGPUVirtualAddress());

    if (model_) {
        // モデルに設定されているテクスチャを t0 に設定する
        commandList->SetGraphicsRootDescriptorTable(
            4,
            TextureManager::GetInstance()->GetSrvHandleGPU(
                model_->GetModelData().material.textureFilePath));
    }

    // 環境マップを t1 に設定する
    commandList->SetGraphicsRootDescriptorTable(
        5, TextureManager::GetInstance()->GetSrvHandleGPU(environmentTextureFilePath_));

    if (model_) {
        model_->DrawInstanced(instanceCount);
    }


    if (model_) {
        model_->DrawInstanced(instanceCount);
    }
}
