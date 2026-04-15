#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "../../math/Math.h"
#include "../../2d/sprite/Sprite.h"
#include "../../2d/texture/TextureManager.h"
#include "../../../game/camera/Camera.h"

class SkyboxCommon;

class Skybox
{
public:
	// 頂点データ構造体
    struct SkyboxVertexData
    {
        Vector4 position;
    };
	// 初期化
    void Initialize(SkyboxCommon* skyboxCommon);
	// 更新
    void Update();
	// 描画
    void Draw();

	// セッター
    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetTextureFilePath(const std::string& filePath) { textureFilePath_ = filePath; }

private:
	// データ作成
    void CreateVertexData();
	// 6面分のインデックスデータを作成
    void CreateIndexData();
	// GPU転送
    void CreateTransformationMatrix();
	// マテリアルデータ作成
    void CreateMaterial();

private:
    // skyboxCommon
    SkyboxCommon* skyboxCommon_ = nullptr;
	// カメラ
    Camera* camera_ = nullptr;

	// GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

	// ビューポートとシザー矩形
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	// CPU側のデータ
    TransformationMatrix* transformationMatrixData_ = nullptr;
    Material* materialData_ = nullptr;

	// 頂点データとインデックスデータ
    std::vector<SkyboxVertexData> vertices_;
    std::vector<uint32_t> indices_;

	// テクスチャ関連
    uint32_t textureIndex_ = 0;
    std::string textureFilePath_ = "Resources/rostock_laage_airport_4k.dds";

    Transform transform_{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };
};
