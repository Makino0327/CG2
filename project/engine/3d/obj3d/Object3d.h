#pragma once
#include <string>
#include <vector>
#include "Math.h"
#include <cassert>
#include "../../2d/sprite/Sprite.h"
#include <fstream>
#include <sstream>
#include "../../../game/camera/Camera.h"
#include "../../animation/Animation.h"
#include "../../animation/Skeleton.h"
#include "../model/ModelStructs.h"
#include "../../animation/SkinCluster.h"

class Object3dCommon;
class Model;
class Camera;

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

struct CameraForGPU {
	Vector3 worldPosition;
	float padding;
};


class Object3d
{
public:
	// 初期化
	void Initialize(Object3dCommon* object3dCommon);

	void Update();

	void Draw();

	void DrawLightImGui();

	void DrawInstanced(UINT instanceCount);

	static MaterialData  LoadMaterialTemplateFile(
		const std::string& directoryPath,
		const std::string& mtlFileName,
		MaterialData& material);

	// 座標変換行列初期化
	void InitializeTransformationMatrix();

	// 
	void InitializeDirectionalLight();
	void InitializeCameraForGPU();

	void SetModel(const std::string& filePath);

	// セッター
	void SetModel(Model* model) { model_ = model; }
	void SetTexture(const std::string& filePath);

	void InitializeMaterial();


	// ----- setter -----
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	// 論理座標を変えず、モデルの原点だけを描画時に補正する。
	void SetModelOffset(const Vector3& offset) { modelOffset_ = offset; }
	void SetCamera(Camera* camera) { camera_ = camera; }	

	// ----- getter -----
	const Vector3& GetScale()     const { return transform.scale; }
	const Vector3& GetRotate()    const { return transform.rotate; }
	const Vector3& GetTranslate() const { return transform.translate; }
	// 骨に付けた銃口などの位置計算にも、描画と同じ足元補正済みの行列を渡す。
	const Matrix4x4& GetWorldMatrix() const { assert(transformationMatrixData_); return transformationMatrixData_->World; }
	Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

	void SetColor(const Vector4& color);
	void SetEnvironmentTexture(const std::string& filePath);
	void SetEnvironmentCoefficient(float coefficient);
	// マテリアル全体は渡さず、ライティングの種類だけを変更できるようにする
	void SetLightingType(LightingType lightingType);

	// このオブジェクトだけにディゾルブを適用するか設定する
	void SetDissolveEnabled(bool enabled);
	// 0.0fで表示、1.0fで完全に消える進行度を設定する
	void SetDissolveThreshold(float threshold);
	// 消える境界線の太さを設定する
	void SetDissolveEdgeWidth(float edgeWidth);
	// 消える境界線の色を設定する
	void SetDissolveEdgeColor(const Vector4& edgeColor);

	// アニメーション関連
	void SetAnimation(const Animation& animation) { animation_ = animation; }
	// 現在表示中の姿勢から次のモーションへつなぐ。移動同士なら足運びの周期も引き継ぐ。
	void TransitionToAnimation(const Animation& animation, float seconds, bool preservePhase);
	void SetIsAnimationPlaying(bool isPlaying) { isAnimationPlaying_ = isPlaying; }
	void ResetAnimationTime() { animationTime_ = 0.0f; }
	void ResetSkeletonPose(); // Skeletonをモデル読み込み時の姿勢に戻す
	void SetAnimationNodeName(const std::string& nodeName) { animationNodeName_ = nodeName; }

	// 指定された骨だけに一度きりの動作を重ね、移動の再生時刻は維持する。
	void PlayAnimationOverlay(const Animation& animation) {
		animationOverlay_ = animation;
		animationOverlayTime_ = 0.0f;
		isAnimationOverlayPlaying_ = animation.duration > 0.0f;
	}
	void StopAnimationOverlay() { isAnimationOverlayPlaying_ = false; }
	// 構えの基準となる親の回転を保存し、移動モーションのひねりを相殺する。
	void SetAnimationParentReference(const std::string& jointName, const Animation& reference);
	void SetAnimationParentCorrectionEnabled(bool enabled) { isAnimationParentCorrectionEnabled_ = enabled; }

	const Skeleton& GetSkeleton() const { return skeleton_; } // Skeleton を参照する
	bool HasSkeleton() const { return hasSkeleton_; }         // Skeleton を持つか返す
	void SetSkeletonVisible(bool visible) { isSkeletonVisible_ = visible; } // Skeleton のデバッグ表示切替
	bool IsSkeletonVisible() const { return isSkeletonVisible_; }           // 表示状態を返す

	const SkinCluster& GetSkinCluster() const { return skinCluster_; }

	bool HasSkinCluster() const { return hasSkinCluster_; }

private:
	// ComputeShader に渡す頂点数情報
	struct SkinningInformationForGPU {
		uint32_t numVertices;
		uint32_t padding[3];
	};

	// ピクセルシェーダーへ渡すオブジェクト単位のディゾルブ設定
	struct DissolveData {
		float threshold;
		float edgeWidth;
		uint32_t isEnabled;
		float padding;
		Vector4 edgeColor;
	};

	// ComputeShader 用の定数バッファを初期化する
	void InitializeSkinningInformation();

	// ComputeShader でスキニングを実行する
	void ApplySkinningCompute();

private:
	// 3Dオブジェクト共通処理
	Object3dCommon* object3dCommon_ = nullptr;

	// バッファリソース（ConstantBuffer）
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	// ライト用の定数バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	// Shared light intensity for all Object3d instances.
	static float lightIntensity_;

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

	Transform transform;
	Vector3 modelOffset_{};
	Transform cameraTransform;

	Model* model_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Camera* camera_ = nullptr;

	// オブジェクトごとに保持するディゾルブ用定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> dissolveResource_;
	DissolveData* dissolveData_ = nullptr;
	std::string environmentTextureFilePath_ = "Resources/skybox.dds";

	Matrix4x4 viewProjectionMatrix_{};

	// アニメーション関連
	Animation animation_;
	std::vector<QuaternionTransform> animationBlendSource_;
	float animationBlendTime_ = 0.0f;
	float animationBlendDuration_ = 0.0f;
	int32_t animationCorrectionJoint_ = -1;
	Quaternion animationReferenceParent_{ 0.0f, 0.0f, 0.0f, 1.0f };
	bool isAnimationParentCorrectionEnabled_ = false;
	Animation animationOverlay_; // 射撃などで上書きする骨だけを持つモーション
	float animationOverlayTime_ = 0.0f;
	bool isAnimationOverlayPlaying_ = false;
	float animationTime_ = 0.0f;
	bool isAnimationPlaying_ = false;
	std::string animationNodeName_;

	Skeleton skeleton_;              // この Object3d が持つ Skeleton
	bool hasSkeleton_ = false;       // Skeleton を持っているかどうか
	bool isSkeletonVisible_ = true;  // デバッグ表示を出すかどうか

	// Skinning 用データ
	SkinCluster skinCluster_;

	// Skinning を持つモデルかどうか
	bool hasSkinCluster_ = false;
	// ComputeShader に渡す頂点数情報の定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> skinningInformationResource_;

	// ComputeShader に渡す頂点数情報の書き込み先
	SkinningInformationForGPU* skinningInformationData_ = nullptr;

};

