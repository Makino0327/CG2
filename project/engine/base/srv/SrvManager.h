#pragma once
#include "../DirectX/DirectXCommon.h"
#include <cassert>

class SrvManager
{
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	uint32_t Allocate();

	bool CanAllocate() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	// === SRV生成（テクスチャ用）===
	void CreateSRVforTexture2D(
		uint32_t srvIndex,
		ID3D12Resource* pResource,
		const DirectX::TexMetadata& metadata);

	// === SRV生成（StructuredBuffer用）===
	void CreateSRVforStructuredBuffer(
		uint32_t srvIndex,
		ID3D12Resource* pResource,
		UINT numElements,
		UINT structureByteStride);

	void PreDraw();

	void SetGraphicsRootDescriptorTable(
		UINT RootParameterIndex, uint32_t srvIndex);

	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

	void CreateSRVForRenderTexture(
		uint32_t srvIndex,
		ID3D12Resource* pResource,
		DXGI_FORMAT format);

private:
	DirectXCommon* directXCommon_ = nullptr;	

	// 最大SRV数 (最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount_;
	// SRV用のでスクリプタサイズ
	uint32_t descriptorSize_;
	// デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	// 次に使用するSRVインデックス
	uint32_t useIndex_ = 0;

	
};

