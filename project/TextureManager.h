#pragma once
#include <string>
#include "externals/DirectXTex/DirectXTex.h"
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include <unordered_map>
#include "SrvManager.h"

class TextureManager
{

public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	DirectX::ScratchImage LoadTexture(const std::string& filePath);

	// SRVインデックスの取得
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	// テクスチャ番号からGPUハンドルを取得
	//D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

	// メタデータを習得
	//const DirectX::TexMetadata& GetMetaData(uint32_t textureIndex);

	// メタデータの取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

	// SRVインデックスの取得
	uint32_t GetSrvIndex(const std::string& filePath);

	// GPUハンドルの取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

	SrvManager* GetSrvManager() const { return srvManager_; }


private:
	static TextureManager* instance_;

	DirectXCommon* dxCommon_ = nullptr;

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	// テクスチャ一枚のデータ
	struct TextureData {
		std::string filePath;
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};
	SrvManager* srvManager_ = nullptr;
public:
	// シングルトンインスタンス取得
	static TextureManager* GetInstance();
	// 終了
	void Finalize();
	// テクスチャデータ
	std::unordered_map<std::string,TextureData> textureDatas_;
	std::vector<std::string> textureOrder_; // ★読み込み順を保持（index→filePath変換用）

};

