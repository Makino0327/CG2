#pragma once
#include "../../base/DirectX/DirectXCommon.h"
#include "../obj3d/Object3d.h"
#include "ModelCommon.h"
class Model
{
public:
	//初期化
	void Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename);

	void Draw();

	// ★★ インスタンシング用 ★★
	void DrawInstanced(UINT instanceCount);

	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	void SetTextureIndex(uint32_t index) { modelData_.material.textureIndex = index; }


private:
	// 頂点バッファ作成
	void InitializeVertexBuffer();
	// マテリアル初期化
	void InitializeMaterial();

private:
	// modelの共通処理
	ModelCommon* modelCommon_;
	// objファイルデータ
	ModelData modelData_;
	// 頂点バッファ群
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> vertexBuffers_;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews_;
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	// バッファリソース（ConstantBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	// バッファリソース内のデータを指すポインタ
	Material* materialData_ = nullptr;
};

