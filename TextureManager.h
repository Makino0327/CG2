#pragma once
#include <string>
#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h" // DirectXTexヘッダーをインクルード

class TextureManager
{
public:
	// テクスチャ読み込み（WIC）
	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

	// GPUリソース作成
	static ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

	// データ転送（WriteToSubresource）
	static void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);
};

