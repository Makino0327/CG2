#pragma once
#include <string>
#include <vector>
#include "Math.h"
#include <cassert>
#include "Sprite.h"
#include <fstream>
#include <sstream>
#include "Camera.h"

class Object3dCommon;
class Model;
class Camera;

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

	void DrawInstanced(UINT instanceCount);

	void InitializeCamera();

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

	void InitializeMaterial();


	// ----- setter -----
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetCamera(Camera* camera) { camera_ = camera; }	

	// ----- getter -----
	const Vector3& GetScale()     const { return transform.scale; }
	const Vector3& GetRotate()    const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }
	Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

	void SetColor(const Vector4& color);
	// Object3d.h（publicに追加）
	Material* GetMaterialData() { return materialData_; }
	DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }


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
	Camera* camera_ = nullptr;

	Matrix4x4 viewProjectionMatrix_{};

	struct CameraForGPU
	{
		Vector3 worldPosition;
		float padding;
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

};

