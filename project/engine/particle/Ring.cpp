#include "Ring.h"

#include <cassert>
#include <cstring>
#include <cmath>
#include <numbers>

void Ring::Initialize(DirectXCommon* dxCommon)
{
    // 引数で受け取った DirectXCommon を保持する
    dxCommon_ = dxCommon;
    assert(dxCommon_);

    // 先に CPU 側で Ring の頂点を作る
    CreateVertexData();

    // その後 GPU の頂点バッファを作る
    CreateVertexBuffer();
}

void Ring::CreateVertexData()
{
    // 前回のデータが残らないように空にする
    vertices_.clear();

    // 1分割あたりの角度
    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / float(divide_);

    // 円周を 1 区間ずつ処理する
    for (uint32_t index = 0; index < divide_; ++index) {
        // 今の角度と次の角度
        float angle = float(index) * radianPerDivide;
        float nextAngle = float(index + 1) * radianPerDivide;

        // 今の角度の sin / cos
        float sinValue = std::sin(angle);
        float cosValue = std::cos(angle);

        // 次の角度の sin / cos
        float sinNext = std::sin(nextAngle);
        float cosNext = std::cos(nextAngle);

        // 円周方向の UV
        float u = float(index) / float(divide_);
        float uNext = float(index + 1) / float(divide_);

        // 4頂点を作る
        VertexData v0{};
        VertexData v1{};
        VertexData v2{};
        VertexData v3{};

        // 位置を設定する
        // XY 平面上に Ring を作る
        v0.position = { -sinValue * outerRadius_,  cosValue * outerRadius_, 0.0f, 1.0f };
        v1.position = { -sinNext * outerRadius_,  cosNext * outerRadius_, 0.0f, 1.0f };
        v2.position = { -sinValue * innerRadius_,  cosValue * innerRadius_, 0.0f, 1.0f };
        v3.position = { -sinNext * innerRadius_,  cosNext * innerRadius_, 0.0f, 1.0f };

        // UV を設定する
        // 外側を v=0、内側を v=1 にする
        v0.texcoord = { u,     0.0f };
        v1.texcoord = { uNext, 0.0f };
        v2.texcoord = { u,     1.0f };
        v3.texcoord = { uNext, 1.0f };

        // 法線は +Z 向きにそろえる
        v0.normal = { 0.0f, 0.0f, 1.0f };
        v1.normal = { 0.0f, 0.0f, 1.0f };
        v2.normal = { 0.0f, 0.0f, 1.0f };
        v3.normal = { 0.0f, 0.0f, 1.0f };

        // pad も 0 にしておく
        v0.pad = 0.0f;
        v1.pad = 0.0f;
        v2.pad = 0.0f;
        v3.pad = 0.0f;

        // 2枚の三角形で 1 区間を作る
        vertices_.push_back(v0);
        vertices_.push_back(v1);
        vertices_.push_back(v2);

        vertices_.push_back(v2);
        vertices_.push_back(v1);
        vertices_.push_back(v3);
    }
}

void Ring::CreateVertexBuffer()
{
    assert(dxCommon_);
    assert(!vertices_.empty());

    // 頂点配列のサイズ分だけバッファを作る
    const size_t bufferSize = sizeof(VertexData) * vertices_.size();
    vertexResource_ = dxCommon_->CreateBufferResource(bufferSize);
    assert(vertexResource_);

    // 頂点データを書き込む
    VertexData* mappedData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    std::memcpy(mappedData, vertices_.data(), bufferSize);
    vertexResource_->Unmap(0, nullptr);

    // 頂点バッファビューを設定する
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Ring::DrawInstanced(uint32_t instanceCount)
{
    assert(dxCommon_);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);

    // Ring の頂点バッファをセットする
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 三角形リストとして描画する
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 1つの Ring メッシュを instanceCount 個描画する
    commandList->DrawInstanced(
        static_cast<UINT>(vertices_.size()),
        instanceCount,
        0,
        0);
}
