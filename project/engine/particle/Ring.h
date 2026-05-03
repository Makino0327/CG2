#pragma once
#include <vector>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "../2d/sprite/Sprite.h"           // VertexData を使う
#include "../base/DirectX/DirectXCommon.h" // DirectXCommon を使う

class Ring
{
public:
    // 初期化。頂点生成と頂点バッファ作成をまとめて行う
    void Initialize(DirectXCommon* dxCommon);

    // インスタンシング描画を行う
    void DrawInstanced(uint32_t instanceCount);

private:
    // Ring の頂点列を CPU 側で作る
    void CreateVertexData();

    // 作成した頂点列を GPU の頂点バッファへ送る
    void CreateVertexBuffer();

private:
    // DirectX の共通管理
    DirectXCommon* dxCommon_ = nullptr;

    // Ring の頂点配列
    std::vector<VertexData> vertices_;

    // 頂点バッファ本体
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;

    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // Ring の分割数。大きいほど円に近づく
    uint32_t divide_ = 32;

    // 外側の半径
    float outerRadius_ = 1.0f;

    // 内側の半径
    float innerRadius_ = 0.2f;
};
