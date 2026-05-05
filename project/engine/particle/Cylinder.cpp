#include "Cylinder.h"

#include <cassert>
#include <cstring>
#include <cmath>
#include <numbers>

void Cylinder::Initialize(DirectXCommon* dxCommon)
{
    // // 引数でもらった DirectXCommon を保存する
    dxCommon_ = dxCommon;
    assert(dxCommon_);

    // // 先に CPU 側で頂点を作る
    CreateVertexData();

    // // そのあと GPU 側の頂点バッファを作る
    CreateVertexBuffer();
}

void Cylinder::CreateVertexData()
{
    // // 以前の頂点データを消して作り直す
    vertices_.clear();

    // // 1分割あたりの角度
    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / float(divide_);

    // // 円周を1周しながら側面を作る
    for (uint32_t index = 0; index < divide_; ++index) {
        // // 今の角度
        float angle = float(index) * radianPerDivide;

        // // 次の角度
        float nextAngle = float(index + 1) * radianPerDivide;

        // // 今の角度の sin / cos
        float sinValue = std::sin(angle);
        float cosValue = std::cos(angle);

        // // 次の角度の sin / cos
        float sinNext = std::sin(nextAngle);
        float cosNext = std::cos(nextAngle);

        // // UV の横方向
        float u = float(index) / float(divide_);
        float uNext = float(index + 1) / float(divide_);

        // // 1区間ぶんの4頂点
        VertexData topLeft{};
        VertexData topRight{};
        VertexData bottomLeft{};
        VertexData bottomRight{};

        // // 上側の頂点
        topLeft.position = { -sinValue * radius_, height_, cosValue * radius_, 1.0f };
        topRight.position = { -sinNext * radius_, height_, cosNext * radius_, 1.0f };

        // // 下側の頂点
        bottomLeft.position = { -sinValue * radius_, 0.0f, cosValue * radius_, 1.0f };
        bottomRight.position = { -sinNext * radius_, 0.0f, cosNext * radius_, 1.0f };

       // V を反転して上下面の見え方を逆にする
        topLeft.texcoord = { u, 1.0f };
        topRight.texcoord = { uNext, 1.0f };
        bottomLeft.texcoord = { u, 0.0f };
        bottomRight.texcoord = { uNext, 0.0f };


        // // 法線は側面の外向き
        topLeft.normal = { -sinValue, 0.0f, cosValue };
        topRight.normal = { -sinNext, 0.0f, cosNext };
        bottomLeft.normal = { -sinValue, 0.0f, cosValue };
        bottomRight.normal = { -sinNext, 0.0f, cosNext };

        // // pad は 0 にしておく
        topLeft.pad = 0.0f;
        topRight.pad = 0.0f;
        bottomLeft.pad = 0.0f;
        bottomRight.pad = 0.0f;

        // // 2枚の三角形で1区間の側面を作る
        vertices_.push_back(topLeft);
        vertices_.push_back(topRight);
        vertices_.push_back(bottomLeft);

        vertices_.push_back(bottomLeft);
        vertices_.push_back(topRight);
        vertices_.push_back(bottomRight);
    }
}

void Cylinder::CreateVertexBuffer()
{
    assert(dxCommon_);
    assert(!vertices_.empty());

    // // 頂点数に合わせたサイズでバッファを作る
    const size_t bufferSize = sizeof(VertexData) * vertices_.size();
    vertexResource_ = dxCommon_->CreateBufferResource(bufferSize);
    assert(vertexResource_);

    // // CPU 側の頂点データを GPU バッファへコピーする
    VertexData* mappedData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    std::memcpy(mappedData, vertices_.data(), bufferSize);
    vertexResource_->Unmap(0, nullptr);

    // // 頂点バッファビューを設定する
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Cylinder::DrawInstanced(uint32_t instanceCount)
{
    assert(dxCommon_);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);

    // // Cylinder の頂点バッファをセットする
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // // 三角形リストとして描画する
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // // 生成した頂点数ぶんを instanceCount 個描画する
    commandList->DrawInstanced(
        static_cast<UINT>(vertices_.size()),
        instanceCount,
        0,
        0);
}
