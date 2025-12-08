#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"  
#include "ModelManager.h"

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    object3dCommon_ = object3dCommon;

	camera_ = object3dCommon_->GetDefaultCamera();

    transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    cameraTransform = { {1.0f,1.0f,1.0f},{0.3f,0.0f,0.0f},{0.0f,3.0f,-10.0f} };

    InitializeTransformationMatrix();
    InitializeDirectionalLight();
    InitializeMaterial();
}
void Object3d::Update()
{
    assert(transformationMatrixData_);

    Matrix4x4 worldMatrix =
        MakeAffineMatrix(transform.scale,
            transform.rotate,
            transform.translate);

    Matrix4x4 worldViewProjectionMatrix;

    if (camera_) {
        const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

        // ★ camera の VP を保持
        viewProjectionMatrix_ = vp;

        worldViewProjectionMatrix = Multiply(worldMatrix, vp);
    } else {
        worldViewProjectionMatrix = worldMatrix;
        viewProjectionMatrix_ = MakeIdentity4x4(); // なくてもいいけど一応
    }

    transformationMatrixData_->WVP = worldViewProjectionMatrix;
    transformationMatrixData_->World = worldMatrix;
}


// Object3d.cpp

void Object3d::Draw()
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // ★Material CBuffer(b0)
    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());

    // 平行光源(b1)
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());

    // 行列(b2)
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());

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
        return material;   // 読めない場合そのまま返す
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
    // Object3dCommon から DirectXCommon を取得
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // 実プロジェクトの関数名に合わせて

    // バッファリソース作成
    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    // マップしてポインタ取得
    transformationMatrixResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));

    // 初期値を書き込む
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}

void Object3d::InitializeDirectionalLight()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    // バッファ作成
    directionalLightResource_ =
        dxCommon->CreateBufferResource(sizeof(DirectionalLight));

    // マップしてポインタ取得
    directionalLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&directionalLightData_));

    // 初期値設定（main.cpp と同じ）
    directionalLightData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f);
    directionalLightData_->intensity = 4.0f;
}

void Object3d::SetModel(const std::string& filePath)
{
    // モデルを検索してセットする
    model_ = ModelManager::GetInstance()->FindModel(filePath);
}

void Object3d::SetTexture(const std::string& filePath)
{
    TextureManager* texMan = TextureManager::GetInstance();
    texMan->LoadTexture(filePath);

    if (model_) {
        model_->SetTextureIndex(
            texMan->GetTextureIndexByFilePath(filePath));
    }
}

void Object3d::InitializeMaterial()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    // 初期値
    materialData_->color = Vector4(1, 1, 1, 1);
    materialData_->lightingType = 1; // Lambertとか使うなら。無ければ0でもOK
    materialData_->uvTransform = MakeIdentity4x4();
}

void Object3d::SetColor(const Vector4& color)
{
    if (materialData_) {
        materialData_->color = color;
    }
}

// Object3d.cpp

void Object3d::DrawInstanced(UINT instanceCount)
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // ★Material CBuffer(b0)
    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());

    // ★平行光源(b1)
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());

    // ★行列(b2)
    // ※「Object3d用のインスタンシング」をする場合は
    //    ここも使う想定だから残す
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());

    if (model_) {
        model_->DrawInstanced(instanceCount); // ★ここだけ違う
    }
}
