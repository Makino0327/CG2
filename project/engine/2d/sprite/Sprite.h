#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../engine/math/Math.h"

#include "../texture/TextureManager.h"

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
	void Initialize(SpriteCommon* spriteCommon, ID3D12Resource* directionalLightResource, std::string textureFilePath);
	// 更新
	void Update();
	// 描画
	void Draw();
	// 頂点データ作成
	void CreateVertexData();
	// マテリアルデータ作成
	void CreateMaterialData();
	//	座標変換行列データ作成
	void CreateTransformationMatrixData();

	void AdjustTextureSize();

	// getter
	const Vector2& GetPosition() const { return position_; }
	float GetRotation() const { return rotation_; }
	const Vector4& GetColor() const { return materialData->color; }
	const Vector2& GetSize() const { return size_; }
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }
	bool GetFlipX() const { return isFlipX_; }
	bool GetFlipY() const { return isFlipY_; }
	const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
	const Vector2& GetTextureSize() const { return textureSize_; }



	// setter
	void SetPosition(const Vector2& position) { position_ = position; }
	void SetRotation(float rotation) { rotation_ = rotation; }
	void SetColor(const Vector4& color) { materialData->color = color; }
	void SetSize(const Vector2& size) { size_ = size; }
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
	void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }
	void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }
	void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop_ = leftTop; }
	void SetTextureSize(const Vector2& size) { textureSize_ = size; }


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

	// テクスチャ番号
	uint32_t textureIndex_ = 0;

	Transform transform_{
	   {1.0f, 1.0f, 1.0f},
	   {0.0f, 0.0f, 0.0f},
	   {0.0f, 0.0f, 0.0f}
	};

	Vector2 position_ = { 0.0f, 0.0f };
	float rotation_ = 0.0f;

	Vector2 size_ = { 100.0f,100.0f };

	// アンカーポイント（0.0～1.0）
	Vector2 anchorPoint_ = { 0.0f, 0.0f };

	// 左右フリップ
	bool isFlipX_ = false;
	// 上下フリップ
	bool isFlipY_ = false;

	// テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f,0.0f };
	// テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 512.0f,512.0f };

	std::string textureFilePath_;

};

