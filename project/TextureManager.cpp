#include "TextureManager.h"

#include "DirectXCommon.h"
#include "Logger.h"

TextureManager* TextureManager::instance_ = nullptr;
//uint32_t TextureManager::kSRVIndexTop = 1; // 0 は ImGui 用に予約

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
    srvManager_= srvManager;
	// SRV数の数と同数
	textureDatas_.reserve(DirectXCommon::kMaxSRVCount);
    textureOrder_.reserve(DirectXCommon::kMaxSRVCount);

}

TextureManager* TextureManager::GetInstance()
{
	if (instance_ == nullptr) {
		instance_ = new TextureManager();
	}
	return instance_;
}

void TextureManager::Finalize()
{
	delete instance_;
	instance_ = nullptr;
}

DirectX::ScratchImage TextureManager::LoadTexture(const std::string& filePath)
{
	if (textureDatas_.contains(filePath)) {
		// 読み込み済みなら重複して読まない
		return DirectX::ScratchImage{};
	}

    assert(srvManager_->CanAllocate());

    // ===========================
    // テクスチャ読み込み
    // ===========================
    DirectX::ScratchImage image{};
    std::wstring filePathW(filePath.begin(), filePath.end());

    Logger::Log("Attempting to load texture: " + filePath);

    HRESULT hr = DirectX::LoadFromWICFile(
        filePathW.c_str(),
        DirectX::WIC_FLAGS_FORCE_SRGB,
        nullptr,
        image);
    if (FAILED(hr)) {
        Logger::Log("Failed to load texture: " + filePath);
        return DirectX::ScratchImage{};
    }

    // ===========================
    // MipMap 作成
    // ===========================
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(
        image.GetImages(),
        image.GetImageCount(),
        image.GetMetadata(),
        DirectX::TEX_FILTER_SRGB,
        0,
        mipImages);
    assert(SUCCEEDED(hr));

    // ===========================
    // TextureData を追加
    // ===========================
    //textureDatas_.resize(textureDatas_.size() + 1);
    TextureData& textureData = textureDatas_[filePath];

    // --- テクスチャデータ書き込み ---
    textureData.filePath = filePath;
    textureData.metadata = mipImages.GetMetadata();
    textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

    // ===========================
    // パターン2: 転送用中間リソースを保存
    // ===========================
    // スライドの
    // textureData.intermediateResource = dxCommon->UploadTextureData(...);
    textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource, mipImages);

    // ここではコマンドリストの実行や待機はしない。
    // main 側で「コマンドリストの実行 → 完了待ち → 中間リソース解放」をまとめて行う。

    // ===========================
// SRV確保（★ここがスライドの変更点）
// ===========================
    textureData.srvIndex = srvManager_->Allocate();

    textureData.srvHandleCPU =
        srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);

    textureData.srvHandleGPU =
        srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);



    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metadata.format,
        static_cast<UINT>(textureData.metadata.mipLevels));

    // ★ 読み込み順を保存（旧index互換のため）
    textureOrder_.push_back(filePath);

    // いまの戻り値型に合わせて mipImages を返す
    return mipImages;
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    auto it = std::find(textureOrder_.begin(), textureOrder_.end(), filePath);
    assert(it != textureOrder_.end());
    return static_cast<uint32_t>(std::distance(textureOrder_.begin(), it));
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
    auto it = textureDatas_.find(filePath);
    assert(it != textureDatas_.end());
    return it->second.srvIndex;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
    auto it = textureDatas_.find(filePath);
    assert(it != textureDatas_.end());
    return it->second.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
    auto it = textureDatas_.find(filePath);
    assert(it != textureDatas_.end());
    return it->second.metadata;
}


