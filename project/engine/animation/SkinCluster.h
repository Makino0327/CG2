#pragma once
#include <array>
#include <span>
#include <utility>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "../math/Math.h"
#include "../3d/model/ModelStructs.h"
#include "Skeleton.h"

class DirectXCommon;
class SrvManager;

const uint32_t kNumMaxInfluence = 4;

struct VertexInfluence {
    // 4本までの Joint から受ける重み
    std::array<float, kNumMaxInfluence> weights;

    // 4本までの Joint index
    std::array<int32_t, kNumMaxInfluence> jointIndices;
};

struct WellForGPU {
    // 頂点位置変換用の行列
    Matrix4x4 skeletonSpaceMatrix;

    // 法線変換用の逆転置行列
    Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinCluster {
    // Joint index 順に inverseBindPoseMatrix を並べた配列
    std::vector<Matrix4x4> inverseBindPoseMatrices;

    // 頂点ごとの influence 情報を入れる Resource
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;

    // influence を頂点入力として渡すための VBV
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView{};

    // influence 用 SRV の index
    uint32_t influenceSrvIndex = 0;

    // influence 用 SRV の CPU / GPU ハンドル
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> influenceSrvHandle{};

    // CPU から influence 情報を書き込むための span
    std::span<VertexInfluence> mappedInfluence;

    // MatrixPalette を保持する Resource
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;

    // CPU から MatrixPalette を更新するための span
    std::span<WellForGPU> mappedPalette;

    // palette 用 SRV の index
    uint32_t paletteSrvIndex = 0;

    // palette 用 SRV の CPU / GPU ハンドル
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle{};

        // ComputeShader が書き込む変形済み頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexResource;

    // 描画で使う変形済み頂点バッファの VBV
    D3D12_VERTEX_BUFFER_VIEW skinnedVertexBufferView{};

    // 変形済み頂点バッファの現在の ResourceState
    D3D12_RESOURCE_STATES skinnedVertexCurrentState = D3D12_RESOURCE_STATE_COMMON;

    // ComputeShader 用の UAV index
    uint32_t skinnedVertexUavIndex = 0;

    // ComputeShader 用の UAV の CPU / GPU ハンドル
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> skinnedVertexUavHandle{};

};

// ModelData 全体から Skinning 対象頂点数を数える
uint32_t GetSkinClusterVertexCount(const ModelData& modelData);

// Skeleton と ModelData から SkinCluster を作る
SkinCluster CreateSkinCluster(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    const Skeleton& skeleton,
    const ModelData& modelData);

// Skeleton の現在姿勢から MatrixPalette を更新する
void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton);
