#include "DebugLine3D.h"
#include <cassert>
#include <cstring>

void DebugLine3D::Initialize(DirectXCommon* dxCommon, Line3DCommon* line3dCommon)
{
    assert(dxCommon);
    assert(line3dCommon);

    // DirectX共通を保存する
    dxCommon_ = dxCommon;

    // 線描画共通設定を保存する
    line3dCommon_ = line3dCommon;

    // 変換行列用バッファを作る
    transformationMatrixResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));

    // 書き込み先を取得する
    transformationMatrixResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));

    // 初期値として単位行列を入れる
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}


void DebugLine3D::Reset()
{
    // 前フレームの線分を消す
    vertices_.clear();
}

void DebugLine3D::AddLine(const Vector3& start, const Vector3& end, const Vector4& color)
{
    DebugLineVertex v0;
    DebugLineVertex v1;

    // 始点の座標を設定する
    v0.position = { start.x, start.y, start.z, 1.0f };

    // 始点の色を設定する
    v0.color = color;

    // 終点の座標を設定する
    v1.position = { end.x, end.y, end.z, 1.0f };

    // 終点の色を設定する
    v1.color = color;

    // 始点を追加する
    vertices_.push_back(v0);

    // 終点を追加する
    vertices_.push_back(v1);
}

void DebugLine3D::Upload()
{
    if (vertices_.empty()) {
        // 描く線が無ければ何もしない
        return;
    }

    assert(dxCommon_);

    // 必要なバッファサイズを求める
    size_t bufferSize = sizeof(DebugLineVertex) * vertices_.size();

    // 頂点バッファを作る
    vertexBuffer_ = dxCommon_->CreateBufferResource(bufferSize);

    DebugLineVertex* mapped = nullptr;

    // 書き込み先を取得する
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));

    // CPU側の頂点データを転送する
    std::memcpy(mapped, vertices_.data(), bufferSize);

    // 書き込みを終了する
    vertexBuffer_->Unmap(0, nullptr);

    // GPUアドレスを設定する
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();

    // バッファ全体のサイズを設定する
    vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);

    // 頂点1個分のサイズを設定する
    vertexBufferView_.StrideInBytes = sizeof(DebugLineVertex);
}

void DebugLine3D::Draw()
{
    if (vertices_.empty()) {
        // 描く線が無ければ何もしない
        return;
    }

    assert(dxCommon_);
    assert(line3dCommon_);
    assert(transformationMatrixData_);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // 線描画の共通設定を行う
    line3dCommon_->CommonDrawSetting();

    // 頂点バッファを設定する
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 行列バッファを設定する
    commandList->SetGraphicsRootConstantBufferView(
        0,
        transformationMatrixResource_->GetGPUVirtualAddress());

    // 線分頂点数ぶん描画する
    commandList->DrawInstanced(static_cast<UINT>(vertices_.size()), 1, 0, 0);
}
void DebugLine3D::SetWVP(const Matrix4x4& worldMatrix, const Matrix4x4& viewProjectionMatrix)
{
    assert(transformationMatrixData_);

    // ワールド行列を保存する
    transformationMatrixData_->World = worldMatrix;

    // WVP を計算して保存する
    transformationMatrixData_->WVP = Multiply(worldMatrix, viewProjectionMatrix);
}

