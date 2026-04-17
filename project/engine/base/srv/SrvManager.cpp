#include "SrvManager.h"

const uint32_t SrvManager::kMaxSRVCount_ = 512;

void SrvManager::Initialize(DirectXCommon* dxCommon)
{
	directXCommon_ = dxCommon;
	// でスクリプタヒープの生成
	descriptorHeap_ = directXCommon_->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		kMaxSRVCount_,
		true);
	// でスクリプターサイズの取得
	descriptorSize_ = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

uint32_t SrvManager::Allocate()
{
	// 上限チェック（資料の assert）
	assert(useIndex_ < kMaxSRVCount_);

	// returnする番号を一旦保存
	uint32_t index = useIndex_;

	// 次のためにインデックスを進める
	useIndex_++;

	// 記録した番号を返す
	return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize_ * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize_ * index);
	return handleGPU;
}

// ------------------------------
// SRV生成（テクスチャ用）
// ------------------------------
void SrvManager::CreateSRVforTexture2D(
	uint32_t srvIndex,
	ID3D12Resource* pResource,
	const DirectX::TexMetadata& metadata)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (metadata.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = UINT(metadata.mipLevels);
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
	}

	directXCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex));
}


// ------------------------------
// SRV生成（StructuredBuffer用）
// ------------------------------
void SrvManager::CreateSRVforStructuredBuffer(
	uint32_t srvIndex,
	ID3D12Resource* pResource,
	UINT numElements,
	UINT structureByteStride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	directXCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::PreDraw()
{
	// 描画用のデスクリプタヒープをセット
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
	directXCommon_->GetCommandList()->SetDescriptorHeaps(
		1,
		descriptorHeaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	directXCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
		RootParameterIndex,
		GetGPUDescriptorHandle(srvIndex));
}

bool SrvManager::CanAllocate() const
{
	// 上限に達していなければ true
	return useIndex_ < kMaxSRVCount_;
}

void SrvManager::CreateSRVForRenderTexture(
	uint32_t srvIndex,
	ID3D12Resource* pResource,
	DXGI_FORMAT format)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	directXCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex));
}
