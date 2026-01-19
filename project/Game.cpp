#include "Game.h"

void Game::Initialize() {
	Framework::Initialize();
	// WindowsAPI初期化
	winApp_ = new WinApp();
	winApp_->Initialize();

	// DirectX初期化
	dxCommon_ = new DirectXCommon();
	dxCommon_->Initialize(winApp_);

	// SrvManager 初期化
	srvManager_ = new SrvManager();
	srvManager_->Initialize(dxCommon_);

	// ★ ここで先に TextureManager を初期化
	TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);

	// SpriteCommon の初期化
	spriteCommon_ = new SpriteCommon();
	spriteCommon_->Initialize(dxCommon_, srvManager_);

	// 3d オブジェクト共通処理
	object3dCommon_ = new Object3dCommon();
	object3dCommon_->Initialize(dxCommon_);

	// ★ カメラ生成 & デフォルトカメラに設定
	camera_ = new Camera();
	camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
	camera_->SetTranslate({ 0.0f, 3.0f, -10.0f });
	object3dCommon_->SetDefaultCamera(camera_);

	// ★ ImGui用にカメラ値を保持（初期値は今セットしてる値と同じにする）
	Vector3 camRotate = { 0.3f, 0.0f, 0.0f };
	Vector3 camTranslate = { 0.0f, 3.0f, -10.0f };
	camera_->SetRotate(camRotate);
	camera_->SetTranslate(camTranslate);

	// 3D オブジェクト（元コードにあったので移植）
	object3d_ = new Object3d();
	object3d_->Initialize(object3dCommon_);

	// モデル共通処理
	modelCommon_ = new ModelCommon();
	modelCommon_->Initialize(dxCommon_);

	// ★ ParticleCommon 初期化
	particleCommon_ = new ParticleCommon();
	particleCommon_->Initialize(dxCommon_, srvManager_);

#ifdef USE_IMGUI
	imguiManager_ = new ImGuiManager();
	imguiManager_->Initialize(winApp_, dxCommon_, srvManager_, srvManager_->GetDescriptorHeap());
#endif

	// 3Dモデルマネージャー
	ModelManager::GetInstance()->Initialize(dxCommon_);
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// （必要なら）テクスチャを事前ロード
	auto texMan = TextureManager::GetInstance();
	texMan->LoadTexture("Resources/uvChecker.png");
	texMan->LoadTexture("Resources/monsterBall.png");
	texMan->LoadTexture("Resources/checkerBoard.png");
	texMan->LoadTexture("Resources/circle.png");
	texMan->LoadTexture("Resources/fence.png");

	particleSystem_ = new ParticleSystem();
	particleSystem_->Initialize(dxCommon_, particleCommon_, camera_, srvManager_, ParticleType::CircleBurst);
	particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });   // エミッタ基準位置

	// 通常モデル用のマテリアルリソースを作成
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->lightingType = static_cast<int32_t>(LightingType::Lambert); // 追加してもOK

	// ライト用の定数バッファリソースを作成
	directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	directionalLightData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f);
	directionalLightData_->intensity = 4.0f;


	// 入力の初期化
	input_ = new Input();
	input_->Initialize(winApp_);

	// Spriteの初期化
	for (uint32_t i = 0; i < 5; ++i) {
		Sprite* sprite = new Sprite();

		std::string texPath;
		if (i % 2 == 0) {
			texPath = "Resources/uvChecker.png";
		} else {
			texPath = "Resources/monsterBall.png";
		}

		sprite->Initialize(spriteCommon_, directionalLightResource_.Get(), texPath);

		Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
		sprite->SetPosition(pos);

		sprites_.push_back(sprite);
	}

	// Object3d を作る（元コードの objA）
	objA_ = new Object3d();
	objA_->Initialize(object3dCommon_);
	objA_->SetModel("sphere.obj");
	objA_->SetTexture("Resources/monsterBall.png");
}

void Game::Update() {
	Framework::Update();
	// メッセージ処理（元 while 内の先頭）
	if (winApp_->ProcessMessage()) {
		endRequest_ = true;
		return;
	}

	input_->Update();

	if (input_->TriggerKey(DIK_0)) {
		OutputDebugStringA("push 0\n");
	}

	// Sprite Update（元コード通り）
	for (Sprite* sprite : sprites_) {
		sprite->Update();
	}

	// ライト方向を正規化（入れてOK）
	auto* light = objA_->GetDirectionalLightData();
	light->direction = Normalize(light->direction);


	// ★ここが重要：反映してからUpdate
	objA_->SetTranslate(sphereTranslate_);
	objA_->SetRotate(sphereRotate_);
	objA_->SetScale(sphereScale_);

	objA_->Update();
	camera_->Update();
	particleSystem_->Update(deltaTime_);

	// スプライトに反映（元コード通り）
	if (!sprites_.empty()) {
		sprites_[0]->SetPosition(spritePos_);
	}
}

void Game::Draw() {

	dxCommon_->PreDraw();

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	(void)commandList; // 元コードにあったので残す（未使用警告回避）

	// ===== スプライト =====
	spriteCommon_->CommonDrawSetting();

	/*for (Sprite* sprite : sprites_) {
		sprite->Draw();
	}*/

	// ======= Draw =======
	object3dCommon_->CommonDrawSetting();
	objA_->Draw();

	// そのあとインスタンス描画（Particle用PSO）
	/*particleCommon_->CommonDrawSetting();
	particleSystem_->Draw();*/

#ifdef USE_IMGUI
	imguiManager_->Begin();

	//==============================
	// Sphere & Shadow(Light)
	//==============================
	ImGui::Begin("Sphere & Shadow(Light)");

	// ---- Sphere Transform（Game側のパラメータ → UpdateでobjA_へ反映される）----
	ImGui::Text("Sphere Transform");
	ImGui::DragFloat3("Translate", &sphereTranslate_.x, 0.01f);
	ImGui::DragFloat3("Rotate", &sphereRotate_.x, 0.01f); // rad想定
	ImGui::DragFloat3("Scale", &sphereScale_.x, 0.01f, 0.0f, 100.0f);

	ImGui::Separator();

	// ---- 3DのMaterial/Lightは「objA_の中」を直接いじる ----
	Material* mat = objA_ ? objA_->GetMaterialData() : nullptr;
	DirectionalLight* light = objA_ ? objA_->GetDirectionalLightData() : nullptr;

	if (mat && light) {

		// ---- Material ----
		ImGui::Text("Material");
		ImGui::ColorEdit4("Mat Color", &mat->color.x,
			ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);

		const char* lightingItems[] = { "None", "Lambert", "HalfLambert" };
		int lighting = mat->lightingType;
		if (ImGui::Combo("Lighting", &lighting, lightingItems, IM_ARRAYSIZE(lightingItems))) {
			mat->lightingType = lighting;
		}

		ImGui::Separator();

		// ---- Directional Light ----
		ImGui::Text("Directional Light");
		ImGui::ColorEdit3("Light Color", &light->color.x);
		ImGui::DragFloat3("Direction", &light->direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 4.0f);

		if (ImGui::Button("No Shadow (Intensity=0)")) {
			light->intensity = 0.0f; // ★ここもobjA_側を変更
		}

	} else {
		ImGui::Text("objA_ / material / light is null");
	}

	ImGui::End();

	imguiManager_->End();
	imguiManager_->Draw();
#endif

	dxCommon_->PostDraw();
}

void Game::Finalize() {

	// 3Dモデルマネージャーの終了
	ModelManager::GetInstance()->Finalize();
	// テクスチャマネージャーの終了
	TextureManager::GetInstance()->Finalize();

	// Sprite
	for (Sprite* sprite : sprites_) {
		delete sprite;
	}
	sprites_.clear();

	delete spriteCommon_; spriteCommon_ = nullptr;
	delete input_; input_ = nullptr;

	// 3D
	delete objA_; objA_ = nullptr;
	delete object3d_; object3d_ = nullptr;
	delete object3dCommon_; object3dCommon_ = nullptr;

	// Model / Particle / Camera
	delete modelCommon_; modelCommon_ = nullptr;
	delete particleCommon_; particleCommon_ = nullptr;
	delete camera_; camera_ = nullptr;
	delete particleSystem_; particleSystem_ = nullptr;

#ifdef USE_IMGUI
	imguiManager_->Finalize();
	delete imguiManager_;
	imguiManager_ = nullptr;
#endif

	delete srvManager_; srvManager_ = nullptr;

	delete dxCommon_; dxCommon_ = nullptr;

	if (winApp_) {
		winApp_->Finalize();
		delete winApp_;
		winApp_ = nullptr;
	}

	Framework::Finalize();
}
