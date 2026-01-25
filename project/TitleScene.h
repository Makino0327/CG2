#pragma once
#include <vector>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "Math.h"
#include "BaseScene.h"

class DirectXCommon;
class SrvManager;
class SpriteCommon;
class Object3dCommon;
class ModelCommon;
class ParticleCommon;
class Camera;
class Sprite;
class Object3d;
class ParticleSystem;

class TitleScene : public BaseScene
{
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
public:
	void Initialize(DirectXCommon* dxCommon,
		SrvManager* srvManager,
		SpriteCommon* spriteCommon,
		Object3dCommon* object3dCommon,
		ModelCommon* modelCommon,
		ParticleCommon* particleCommon,
		Camera* camera);

	void Update(float deltaTime);

private:
	// ===== 共通参照（Gameが持つ）=====
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Object3dCommon* object3dCommon_ = nullptr;
	ModelCommon* modelCommon_ = nullptr;
	ParticleCommon* particleCommon_ = nullptr;
	Camera* camera_ = nullptr;

	// ===== シーン固有（Gameから移植してここで持つ）=====
	Object3d* object3d_ = nullptr;
	Object3d* objA_ = nullptr;

	std::vector<Sprite*> sprites_;

	ParticleSystem* particleSystem_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

	// Game側で使ってたのと同じ変数（Update/ImGuiで使うならここに）
	Vector2 spritePos_ = { 100.0f, 100.0f };

};

