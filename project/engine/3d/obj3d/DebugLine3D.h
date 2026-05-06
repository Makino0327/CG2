#pragma once
#include <vector>
#include "../../math/Math.h"
#include "../../base/DirectX/DirectXCommon.h"
#include "Line3DCommon.h"
#include "../../2d/sprite/Sprite.h"


struct DebugLineVertex {
    // 線分頂点の座標
    Vector4 position;

    // 頂点カラー
    Vector4 color;
};

class DebugLine3D {
public:
    void Initialize(DirectXCommon* dxCommon, Line3DCommon* line3dCommon);


    // 今フレームの線分を消す
    void Reset();

    // 線分を1本追加する
    void AddLine(const Vector3& start, const Vector3& end, const Vector4& color);

    // CPU側の線分データをGPUへ転送する
    void Upload();

    // 登録済みの線分を描画する
    void Draw();

    void SetWVP(const Matrix4x4& worldMatrix, const Matrix4x4& viewProjectionMatrix);


private:
    // DirectX共通の参照
    DirectXCommon* dxCommon_ = nullptr;

    // CPU側で保持する線分頂点
    std::vector<DebugLineVertex> vertices_;

    // 線描画用の頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;

    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // 変換行列用の定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;

    // 書き込み先
    TransformationMatrix* transformationMatrixData_ = nullptr;

    // 線描画共通設定
    Line3DCommon* line3dCommon_ = nullptr;

};
