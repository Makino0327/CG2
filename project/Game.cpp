#include "Game.h"

bool Game::Initialize() {

    // -----------------------------
    // Windows
    // -----------------------------
    winApp_ = new WinApp();
    winApp_->Initialize();

    // -----------------------------
    // DirectX
    // -----------------------------
    dxCommon_ = new DirectXCommon();
    dxCommon_->Initialize(winApp_);

    // -----------------------------
    // SRV
    // -----------------------------
    srvManager_ = new SrvManager();
    srvManager_->Initialize(dxCommon_);

    // -----------------------------
    // Texture
    // -----------------------------
    TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);

    // -----------------------------
    // Sprite
    // -----------------------------
    spriteCommon_ = new SpriteCommon();
    spriteCommon_->Initialize(dxCommon_, srvManager_);

    // -----------------------------
    // Object3D
    // -----------------------------
    object3dCommon_ = new Object3dCommon();
    object3dCommon_->Initialize(dxCommon_);

    // Camera
    camera_ = new Camera();
    camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 3.0f, -10.0f });
    object3dCommon_->SetDefaultCamera(camera_);

    // Object3d
    object3d_ = new Object3d();
    object3d_->Initialize(object3dCommon_);

    // -----------------------------
    // Model
    // -----------------------------
    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(dxCommon_);

    ModelManager::GetInstance()->Initialize(dxCommon_);
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");

    // -----------------------------
    // Particle
    // -----------------------------
    particleCommon_ = new ParticleCommon();
    particleCommon_->Initialize(dxCommon_, srvManager_);

    particleSystem_ = new ParticleSystem();
    particleSystem_->Initialize(
        dxCommon_,
        particleCommon_,
        camera_,
        srvManager_,
        ParticleType::CircleBurst
    );
    particleSystem_->SetPosition({ 0.0f, 0.0f, 0.0f });

    // -----------------------------
    // ImGui
    // -----------------------------
#ifdef USE_IMGUI
    imguiManager_ = new ImGuiManager();
    imguiManager_->Initialize(
        winApp_,
        dxCommon_,
        srvManager_,
        srvManager_->GetDescriptorHeap()
    );
#endif
    // 通常モデル用のマテリアルリソースを作成
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    materialData->uvTransform = MakeIdentity4x4();
    // ライト用の定数バッファリソースを作成
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    // 書き込み用ポインタの定義（これが必要）
    DirectionalLight* directionalLightData = nullptr;
    // マップしてアドレス取得
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    // 初期化（単位ベクトルで）
    directionalLightData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    directionalLightData->direction = Vector3(0.0f, -1.0f, 0.0f); // 正規化されてること
    directionalLightData->intensity = 4.0f;

    // -----------------------------
    // Input
    // -----------------------------
    input_ = new Input();
    input_->Initialize(winApp_);

    // -----------------------------
    // Texture preload
    // -----------------------------
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle.png");
    texMan->LoadTexture("Resources/fence.png");

    // -----------------------------
    // Sprite生成
    // -----------------------------
    for (uint32_t i = 0; i < 5; ++i) {
        Sprite* sprite = new Sprite();

        std::string texPath =
            (i % 2 == 0)
            ? "Resources/uvChecker.png"
            : "Resources/monsterBall.png";

        sprite->Initialize(spriteCommon_, nullptr, texPath);
        sprite->SetPosition({ 100.0f + 150.0f * i, 200.0f });

        sprites_.push_back(sprite);
    }

    return true;
}
