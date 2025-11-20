#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"  

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    object3dCommon_ = object3dCommon;

    transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    cameraTransform = { {1.0f,1.0f,1.0f},{0.3f,0.0f,0.0f},{0.0f,4.0f,-10.0f} };

    InitializeTransformationMatrix();
    InitializeDirectionalLight();
}

void Object3d::Update()
{
    assert(transformationMatrixData_);   // 初期化済み前提

    // ① Transform → WorldMatrix
    Matrix4x4 worldMatrix =
        MakeAffineMatrix(transform.scale,
            transform.rotate,
            transform.translate);

    // ② cameraTransform → cameraMatrix
    Matrix4x4 cameraMatrix =
        MakeAffineMatrix(cameraTransform.scale,
            cameraTransform.rotate,
            cameraTransform.translate);

    // ③ cameraMatrix → viewMatrix（逆行列）
    Matrix4x4 viewMatrix = Inverse(cameraMatrix);

    // ④ projectionMatrix（射影行列）
    Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
        0.45f,
        float(WinApp::kClientWidth) / float(WinApp::kClientHeight),
        0.1f, 100.0f);

    // ⑤ WVP と World を定数バッファに書き込む
    transformationMatrixData_->WVP =
        Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
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


MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath,
    const std::string& filename) {
    MaterialData material{};
    material.textureFilePath = directoryPath + "/" + filename; // 必要なら
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
