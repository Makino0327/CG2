#include "SkinCluster.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "../base/directX/DirectXCommon.h"
#include "../base/srv/SrvManager.h"

namespace {

    // Matrix4x4 の転置行列を作る
    Matrix4x4 TransposeMatrix(const Matrix4x4& matrix)
    {
        Matrix4x4 result{};

        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                result.m[row][column] = matrix.m[column][row];
            }
        }

        return result;
    }

}

uint32_t GetSkinClusterVertexCount(const ModelData& modelData)
{
    uint32_t vertexCount = 0;

    // すべての mesh の頂点数を合計する
    for (const MeshData& mesh : modelData.meshes) {
        vertexCount += static_cast<uint32_t>(mesh.vertices.size());
    }

    return vertexCount;
}

SkinCluster CreateSkinCluster(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    const Skeleton& skeleton,
    const ModelData& modelData)
{
    assert(dxCommon);
    assert(srvManager);
    assert(!skeleton.joints.empty());

    SkinCluster skinCluster{};

    const uint32_t jointCount = static_cast<uint32_t>(skeleton.joints.size());
    const uint32_t vertexCount = GetSkinClusterVertexCount(modelData);

    assert(vertexCount > 0);

    // palette 用の Resource を作る
    skinCluster.paletteResource =
        dxCommon->CreateBufferResource(sizeof(WellForGPU) * jointCount);

    WellForGPU* mappedPalette = nullptr;
    skinCluster.paletteResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedPalette));

    // CPU から安全にアクセスできるように span へ束ねる
    skinCluster.mappedPalette = { mappedPalette, jointCount };

    // palette 用の SRV を確保する
    skinCluster.paletteSrvIndex = srvManager->Allocate();
    skinCluster.paletteSrvHandle.first =
        srvManager->GetCPUDescriptorHandle(skinCluster.paletteSrvIndex);
    skinCluster.paletteSrvHandle.second =
        srvManager->GetGPUDescriptorHandle(skinCluster.paletteSrvIndex);

    // StructuredBuffer として palette を参照できるようにする
    srvManager->CreateSRVforStructuredBuffer(
        skinCluster.paletteSrvIndex,
        skinCluster.paletteResource.Get(),
        jointCount,
        sizeof(WellForGPU));

    // influence 用の Resource を作る
    skinCluster.influenceResource =
        dxCommon->CreateBufferResource(sizeof(VertexInfluence) * vertexCount);

    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedInfluence));

    skinCluster.mappedInfluence = { mappedInfluence, vertexCount };

    // まず全部を空にする
    for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        for (uint32_t influenceIndex = 0; influenceIndex < kNumMaxInfluence; ++influenceIndex) {
            skinCluster.mappedInfluence[vertexIndex].weights[influenceIndex] = 0.0f;
            skinCluster.mappedInfluence[vertexIndex].jointIndices[influenceIndex] = -1;
        }
    }

    // influence 用 VBV を設定する
    skinCluster.influenceBufferView.BufferLocation =
        skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes =
        UINT(sizeof(VertexInfluence) * vertexCount);
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

    // inverseBindPoseMatrix を Joint 数ぶん確保して単位行列で初期化する
    skinCluster.inverseBindPoseMatrices.resize(jointCount);
    std::fill(
        skinCluster.inverseBindPoseMatrices.begin(),
        skinCluster.inverseBindPoseMatrices.end(),
        MakeIdentity4x4());

    // ModelData 側の Skinning 情報を解析して influence を埋める
    for (const auto& jointWeight : modelData.skinClusterData) {
        // joint 名から Skeleton 内の Joint index を探す
        auto it = skeleton.jointMap.find(jointWeight.first);
        if (it == skeleton.jointMap.end()) {
            // Skeleton に存在しない名前なら無視する
            continue;
        }

        const int32_t jointIndex = it->second;
        assert(jointIndex >= 0);
        assert(static_cast<size_t>(jointIndex) < skinCluster.inverseBindPoseMatrices.size());

        // 対応 Joint の inverseBindPoseMatrix を保存する
        skinCluster.inverseBindPoseMatrices[jointIndex] =
            jointWeight.second.inverseBindPoseMatrix;

        // この Joint が影響を与える頂点群を反映する
        for (const VertexWeightData& vertexWeight : jointWeight.second.vertexWeights) {
            if (vertexWeight.vertexIndex >= skinCluster.mappedInfluence.size()) {
                // 範囲外の頂点番号なら無視する
                continue;
            }

            VertexInfluence& currentInfluence =
                skinCluster.mappedInfluence[vertexWeight.vertexIndex];

            // 空いている influence slot に weight と joint index を入れる
            for (uint32_t influenceIndex = 0; influenceIndex < kNumMaxInfluence; ++influenceIndex) {
                if (currentInfluence.weights[influenceIndex] == 0.0f) {
                    currentInfluence.weights[influenceIndex] = vertexWeight.weight;
                    currentInfluence.jointIndices[influenceIndex] = jointIndex;
                    break;
                }
            }
        }
    }

    // 初期状態の MatrixPalette を一度作っておく
    UpdateSkinCluster(skinCluster, skeleton);

    return skinCluster;
}

void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton)
{
    assert(skeleton.joints.size() <= skinCluster.inverseBindPoseMatrices.size());
    assert(skeleton.joints.size() <= skinCluster.mappedPalette.size());

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        // inverseBindPose と現在の skeletonSpace を掛けて
        // Skinning 用の最終行列を作る
        Matrix4x4 skinningMatrix = Multiply(
            skinCluster.inverseBindPoseMatrices[jointIndex],
            skeleton.joints[jointIndex].skeletonSpaceMatrix);

        skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
            skinningMatrix;

        // 法線用には逆転置行列を入れる
        skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix =
            TransposeMatrix(Inverse(skinningMatrix));
    }
}
