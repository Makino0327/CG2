#pragma once
#include "../../base/DirectX/DirectXCommon.h"
#include "ModelStructs.h"
#include "ModelCommon.h"

class Model
{
public:
	//初期化
	void Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename);

	void Draw();

	// ★★ インスタンシング用 ★★
	void DrawInstanced(UINT instanceCount);

	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadGltfFile(const std::string& directoryPath, const std::string& filename);

	void SetTextureIndex(uint32_t index) { modelData_.material.textureIndex = index; }

	// 読み込んだモデルデータを参照する
	const ModelData& GetModelData() const { return modelData_; }

private:
	// 頂点バッファ作成
	void InitializeVertexBuffer();
	// マテリアル初期化
	void InitializeMaterial();
	void InitializeIndexBuffer();

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

	// 各 mesh 用の index buffer
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> indexBuffers_;

	// 各 mesh 用の index buffer view
	std::vector<D3D12_INDEX_BUFFER_VIEW> indexBufferViews_;

};

