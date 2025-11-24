#include "TextureManager.h"

#include "DirectXCommon.h"
#include "Logger.h"

TextureManager* TextureManager::instance_ = nullptr;

uint32_t TextureManager::kSRVIndexTop = 1; // 0 は ImGui 用に予約

void TextureManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	// SRV数の数と同数
	textureDatas_.reserve(DirectXCommon::kMaxSRVCount);
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
    // ===========================
    // 読み込み済みテクスチャを検索（Todo①）
    // ===========================
    auto it = std::find_if(
        textureDatas_.begin(),
        textureDatas_.end(),
        [&](TextureData& textureData) { return textureData.filePath == filePath; }
    );
    if (it != textureDatas_.end()) {
        // 読み込み済みなら重複して読まない
        return DirectX::ScratchImage{};
    }

    // テクスチャ枚数上限チェック
    assert(textureDatas_.size() < DirectXCommon::kMaxSRVCount);

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
    textureDatas_.resize(textureDatas_.size() + 1);
    TextureData& textureData = textureDatas_.back();

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
    // デスクリプタハンドル計算
    // ===========================
    uint32_t srvIndex =
        static_cast<uint32_t>(textureDatas_.size() - 1) + kSRVIndexTop;

    // SRVのincrementサイズ
    uint32_t descriptorSizeSRV =
        dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ★ dxCommon が持つ共有SRVヒープを使う
    ID3D12DescriptorHeap* heap = dxCommon_->GetSrvDescriptorHeap();

    textureData.srvHandleCPU =
        dxCommon_->GetCPUDescriptorHandle(heap, descriptorSizeSRV, srvIndex);

    textureData.srvHandleGPU =
        dxCommon_->GetGPUDescriptorHandle(heap, descriptorSizeSRV, srvIndex);


    // ===========================
    // SRV の生成
    // ===========================
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureData.metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels =
        static_cast<UINT>(textureData.metadata.mipLevels);

    ID3D12Device* device = dxCommon_->GetDevice();
    device->CreateShaderResourceView(
        textureData.resource.Get(),
        &srvDesc,
        textureData.srvHandleCPU);

    // いまの戻り値型に合わせて mipImages を返す
    return mipImages;
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    // 読み込み済みテクスチャデータを検索（スライド23と同じ find_if）
    auto it = std::find_if(
        textureDatas_.begin(),
        textureDatas_.end(),
        [&](const TextureData& textureData) {
            return textureData.filePath == filePath;
        });

    if (it != textureDatas_.end()) {
        // 見つかった要素の「何番目か」を index として返す
        uint32_t textureIndex =
            static_cast<uint32_t>(std::distance(textureDatas_.begin(), it));
        return textureIndex;
    }

    // ここに来るということは事前に LoadTexture していない → バグなので止める
    assert(false && "Texture not loaded for this filePath");
    return 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
    // 範囲外チェック（スライドの assert）
    assert(textureIndex < textureDatas_.size());

    // テクスチャデータの参照を取得
    const TextureData& data = textureDatas_[textureIndex];

    // GPUハンドルを返す
    return data.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(uint32_t textureIndex)
{
    // 範囲外チェック（スライド通り）
    assert(textureIndex < textureDatas_.size());

    // テクスチャデータを取得
    const TextureData& textureData = textureDatas_[textureIndex];

    // メタデータを返す
    return textureData.metadata;
}
