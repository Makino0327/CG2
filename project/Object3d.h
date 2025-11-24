#pragma once
#include <string>
#include <vector>
#include "Math.h"
#include <cassert>
#include "Sprite.h"
#include <fstream>
#include <sstream>

class Object3dCommon;
class Model;

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

	static MaterialData  LoadMaterialTemplateFile(
		const std::string& directoryPath,
		const std::string& mtlFileName,
		MaterialData& material);

	// 座標変換行列初期化
	void InitializeTransformationMatrix();

	// 
	void InitializeDirectionalLight();

	void SetModel(const std::string& filePath);

	// セッター
	void SetModel(Model* model) { model_ = model; }
	void SetTexture(const std::string& filePath);


	// ----- setter -----
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }

	// ----- getter -----
	const Vector3& GetScale()     const { return transform.scale; }
	const Vector3& GetRotate()    const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }

	void SetColor(const Vector4& color);
	Material* GetMaterial() { return materialData_; } // ImGui用に欲しければ

private:
	// 3Dオブジェクト共通処理
	Object3dCommon* object3dCommon_ = nullptr;

	// バッファリソース（ConstantBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	// ライト用の定数バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	Transform transform;
	Transform cameraTransform;

	Model* model_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	void InitializeMaterial();
};

