#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

#include "Math.h"

class SpriteCommon;

// 頂点データ構造体
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	float pad;
};

// マテリアルデータ構造体
struct Material {
	Vector4 color;
	int32_t lightingType;     // ← ここをリネーム
	float padding[3];         // ← 既にパディング済みなのでそのままOK
	Matrix4x4 uvTransform;
};

// 変換行列構造体
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

class Sprite
{
public:
	// 初期化
	void Initialize(SpriteCommon* spriteCommon, ID3D12Resource* directionalLightResource);
	// 更新
	void Update();
	// 描画
	void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrv);
	// 頂点データ作成
	void CreateVertexData();
	// マテリアルデータ作成
	void CreateMaterialData();
	//	座標変換行列データ作成
	void CreateTransformationMatrixData();

private:
	SpriteCommon* spriteCommon_=nullptr;

	// ライト情報リソース（ConstantBuffer）
	ID3D12Resource* directionalLightResource_ = nullptr;

	// バッファリソース（VertexBuffer / IndexBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	uint32_t* indexData = nullptr;

	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW  indexBufferView{};

	// バッファリソース（ConstantBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;

	// バッファリソース内のデータを指すポインタ
	Material* materialData = nullptr;

	// バッファリソース（ConstantBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData = nullptr;

	Transform transform_{
	   {1.0f, 1.0f, 1.0f},
	   {0.0f, 0.0f, 0.0f},
	   {0.0f, 0.0f, 0.0f}
	};
};

