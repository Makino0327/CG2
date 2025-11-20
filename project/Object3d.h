#pragma once
#include <string>
#include <vector>
#include "Math.h"
#include <cassert>
#include "Sprite.h"
#include <fstream>
#include <sstream>

class Object3dCommon;

struct MaterialData
{
	std::string textureFilePath;
	uint32_t textureIndex = 0;
};

// 頂点データ構造体
struct MeshData {
	std::string name;
	std::vector<VertexData> vertices;
};
// モデルデータ構造体
struct ModelData {
	MaterialData material;
	std::vector<MeshData> meshes;
};

// Lightingの方式を定義する列挙型
enum class LightingType {
	None = 0,
	Lambert,
	HalfLambert
};

struct DirectionalLight {
	Vector4 color;        // ライトの色
	Vector3 direction;    // ライトの向き（単位ベクトル）
	float intensity;      // 強度
};


class Object3d
{
public:
	// 初期化
	void Initialize(Object3dCommon* object3dCommon);

	void Update();

	void Draw();

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
		const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	// 座標変換行列初期化
	void InitializeTransformationMatrix();

	// 頂点バッファ作成
	void InitializeVertexBuffer();
	// マテリアル初期化
	void InitializeMaterial();
	// 
	void InitializeDirectionalLight();

	Transform& GetTransform() { return transform; }

private:
	// 3Dオブジェクト共通処理
	Object3dCommon* object3dCommon_ = nullptr;

	// objファイルデータ
	ModelData modelData_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;

	// 頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	// バッファリソース（ConstantBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	// バッファリソース内のデータを指すポインタ
	Material* materialData_ = nullptr;

	// バッファリソース（ConstantBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	// ライト用の定数バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	Transform transform;
	Transform cameraTransform;
};

