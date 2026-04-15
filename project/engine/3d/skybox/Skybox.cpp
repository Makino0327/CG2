#include "Skybox.h"
#include "SkyboxCommon.h"
#include "../../base/DirectX/DirectXCommon.h"
#include <cassert>
#include <cstring>

void Skybox::Initialize(SkyboxCommon* skyboxCommon)
{
    assert(skyboxCommon);
    skyboxCommon_ = skyboxCommon;

    if (!camera_) {
        camera_ = skyboxCommon_->GetDefaultCamera();
    }

    CreateVertexData();
    CreateIndexData();
    CreateTransformationMatrix();
    CreateMaterial();

    TextureManager* textureManager = TextureManager::GetInstance();
    textureManager->LoadTexture(textureFilePath_);
    textureIndex_ = textureManager->GetSrvIndex(textureFilePath_);
}

void Skybox::Update()
{
    assert(camera_);
    assert(transformationMatrixData_);

    transform_.translate = camera_->GetTranslate();

    Matrix4x4 worldMatrix = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate);

    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();
    Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);

    transformationMatrixData_->World = worldMatrix;
    transformationMatrixData_->WVP = worldViewProjectionMatrix;
}

void Skybox::Draw()
{
    assert(skyboxCommon_);

    DirectXCommon* dxCommon = skyboxCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    skyboxCommon_->CommonDrawSetting();

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    commandList->SetGraphicsRootConstantBufferView(
        0, transformationMatrixResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        1, materialResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootDescriptorTable(
        2,
        TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

    commandList->DrawIndexedInstanced(UINT(indices_.size()), 1, 0, 0, 0);
}

void Skybox::CreateVertexData()
{
    DirectXCommon* dxCommon = skyboxCommon_->GetDxCommon();

    vertices_ = {
        {{-1.0f,  1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f, -1.0f, 1.0f}}, {{-1.0f, -1.0f, -1.0f, 1.0f}}, {{ 1.0f, -1.0f, -1.0f, 1.0f}},
        {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{-1.0f,  1.0f,  1.0f, 1.0f}}, {{ 1.0f, -1.0f,  1.0f, 1.0f}}, {{-1.0f, -1.0f,  1.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f, 1.0f}}, {{-1.0f,  1.0f, -1.0f, 1.0f}}, {{-1.0f, -1.0f,  1.0f, 1.0f}}, {{-1.0f, -1.0f, -1.0f, 1.0f}},
        {{ 1.0f,  1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{ 1.0f, -1.0f, -1.0f, 1.0f}}, {{ 1.0f, -1.0f,  1.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f, 1.0f}}, {{ 1.0f,  1.0f,  1.0f, 1.0f}}, {{-1.0f,  1.0f, -1.0f, 1.0f}}, {{ 1.0f,  1.0f, -1.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f, 1.0f}}, {{ 1.0f, -1.0f, -1.0f, 1.0f}}, {{-1.0f, -1.0f,  1.0f, 1.0f}}, {{ 1.0f, -1.0f,  1.0f, 1.0f}},
    };

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(SkyboxVertexData) * vertices_.size());

    SkyboxVertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, vertices_.data(), sizeof(SkyboxVertexData) * vertices_.size());

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(SkyboxVertexData) * vertices_.size());
    vertexBufferView_.StrideInBytes = sizeof(SkyboxVertexData);
}

void Skybox::CreateIndexData()
{
    DirectXCommon* dxCommon = skyboxCommon_->GetDxCommon();

    indices_ = {
        0, 1, 2, 2, 1, 3,
        4, 5, 6, 6, 5, 7,
        8, 9, 10, 10, 9, 11,
        12, 13, 14, 14, 13, 15,
        16, 17, 18, 18, 17, 19,
        20, 21, 22, 22, 21, 23,
    };

    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices_.size());

    uint32_t* indexData = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
    std::memcpy(indexData, indices_.data(), sizeof(uint32_t) * indices_.size());

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices_.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void Skybox::CreateTransformationMatrix()
{
    DirectXCommon* dxCommon = skyboxCommon_->GetDxCommon();

    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}

void Skybox::CreateMaterial()
{
    DirectXCommon* dxCommon = skyboxCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->lightingType = 0;
    materialData_->padding[0] = 0.0f;
    materialData_->padding[1] = 0.0f;
    materialData_->padding[2] = 0.0f;
    materialData_->uvTransform = MakeIdentity4x4();
}