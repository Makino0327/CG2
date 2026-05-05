#pragma once
#include <vector>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "../2d/sprite/Sprite.h"           // // VertexData を使う
#include "../base/DirectX/DirectXCommon.h" // // DirectXCommon を使う

class Cylinder
{
public:
    // // 初期化して頂点データと頂点バッファを作る
    void Initialize(DirectXCommon* dxCommon);

    // // インスタンシング描画を行う
    void DrawInstanced(uint32_t instanceCount);

private:
    // // Cylinder の頂点を CPU 側で作る
    void CreateVertexData();

    // // 作成した頂点を GPU の頂点バッファへ送る
    void CreateVertexBuffer();

private:
    // // DirectX の共通情報
    DirectXCommon* dxCommon_ = nullptr;

    // // Cylinder の頂点配列
    std::vector<VertexData> vertices_;

    // // 頂点バッファ本体
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;

    // // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // // 円周の分割数
    uint32_t divide_ = 32;

    // // 半径
    float radius_ = 1.0f;

    // // 高さ
    float height_ = 3.0f;
};
