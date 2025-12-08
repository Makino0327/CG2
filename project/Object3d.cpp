#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"  
#include "ModelManager.h"

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    object3dCommon_ = object3dCommon;

    transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    camera_ = object3dCommon_->GetDefaultCamera();

    InitializeTransformationMatrix();
    InitializeDirectionalLight();
}

void Object3d::Update()
{
    assert(transformationMatrixData_);

    // ① Transform → WorldMatrix
    Matrix4x4 worldMatrix =
        MakeAffineMatrix(transform.scale,
            transform.rotate,
            transform.translate);

    // ② Camera の ViewProjection を使う
    Matrix4x4 wvpMatrix = worldMatrix;

    if (camera_) {
        const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
        wvpMatrix = Multiply(worldMatrix, vp);
    }

    // ③ 定数バッファに書き込む
    transformationMatrixData_->WVP = wvpMatrix;
    transformationMatrixData_->World = worldMatrix;
}


// Object3d.cpp

void Object3d::Draw()
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // ---------- 平行光源 CBuffer の場所を設定 ----------
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());

    // ---------- 座標変換行列 CBuffer の場所を設定 ----------
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());

    if (model_)
    {
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
