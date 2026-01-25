#include "Game.h"
#include "GamePlayScene.h"

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

    // 3Dモデルマネージャー（Initializeは共通なのでGameに残す）
    ModelManager::GetInstance()->Initialize(dxCommon_);

    // 入力の初期化（共通なのでGameに残す）
    input_ = new Input();
    input_->Initialize(winApp_);

    // ★ GamePlayScene を生成して、シーン固有の初期化を移植
    gamePlayScene_ = new GamePlayScene();
    gamePlayScene_->Initialize(dxCommon_, srvManager_, spriteCommon_, object3dCommon_, modelCommon_, particleCommon_, camera_);
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

    // ★ シーン側へ委譲
    gamePlayScene_->Update(deltaTime_);
}

void Game::Draw() {

    dxCommon_->PreDraw();

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    (void)commandList; // 元コードにあったので残す（未使用警告回避）

    // ★ シーン側へ委譲（CommonDrawSetting等もシーン側でやる）
    gamePlayScene_->Draw();

#ifdef USE_IMGUI
    imguiManager_->Begin();

    // ★ ImGuiもシーン側へ委譲（元コードのウィンドウそのまま）
   // gamePlayScene_->DrawImGui();

    imguiManager_->End();
    imguiManager_->Draw();
#endif

    dxCommon_->PostDraw();
}

void Game::Finalize() {

    // ★ シーン終了
    if (gamePlayScene_) {
        gamePlayScene_->Finalize();
        delete gamePlayScene_;
        gamePlayScene_ = nullptr;
    }

    // 3Dモデルマネージャーの終了
    ModelManager::GetInstance()->Finalize();
    // テクスチャマネージャーの終了
    TextureManager::GetInstance()->Finalize();

    delete spriteCommon_; spriteCommon_ = nullptr;
    delete input_; input_ = nullptr;

    // 3D
    delete object3d_; object3d_ = nullptr;
    delete object3dCommon_; object3dCommon_ = nullptr;

    // Model / Particle / Camera
    delete modelCommon_; modelCommon_ = nullptr;
    delete particleCommon_; particleCommon_ = nullptr;
    delete camera_; camera_ = nullptr;

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
