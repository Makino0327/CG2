#include "Game.h"
#include "../scene/gameplay/GamePlayScene.h"
#include "../scene/title/TitleScene.h"

#include <memory> // make_unique

void Game::Initialize() {
    Framework::Initialize();

    // WindowsAPI初期化
    winApp_ = std::make_unique<WinApp>();
    winApp_->Initialize();

    // DirectX初期化
    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(winApp_.get());

    // SrvManager 初期化
    srvManager_ = std::make_unique<SrvManager>();
    srvManager_->Initialize(dxCommon_.get());

    // ★ ここで先に TextureManager を初期化
    TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    // SpriteCommon の初期化
    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_.get(), srvManager_.get());

    // 3d オブジェクト共通を初期化する
    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_.get(), srvManager_.get());


    // ★ カメラ生成 & デフォルトカメラに設定
    camera_ = std::make_unique<Camera>();
    // カメラを下向きにして上から見る
    camera_->SetRotate({ 1.2f, 0.0f, 0.0f });

    // 高い位置から少し手前に置く
    camera_->SetTranslate({ 0.0f, 12.0f, -6.0f });

    object3dCommon_->SetDefaultCamera(camera_.get());

    // ImGui用の初期回転も合わせる
    Vector3 camRotate = { 1.2f, 0.0f, 0.0f };

    // ImGui用の初期位置も合わせる
    Vector3 camTranslate = { 0.0f, 12.0f, -6.0f };

    camera_->SetRotate(camRotate);
    camera_->SetTranslate(camTranslate);

    // 3D オブジェクト（元コードにあったので移植）
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon_.get());

    // モデル共通処理
    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(dxCommon_.get(), srvManager_.get());


    // ★ ParticleCommon 初期化
    particleCommon_ = std::make_unique<ParticleCommon>();
    particleCommon_->Initialize(dxCommon_.get(), srvManager_.get());

#ifdef USE_IMGUI
    imguiManager_ = std::make_unique<ImGuiManager>();
    imguiManager_->Initialize(
        winApp_.get(),
        dxCommon_.get(),
        srvManager_.get(),
        srvManager_->GetDescriptorHeap()
    );
#endif

    // 3Dモデルマネージャー（Initializeは共通なのでGameに残す）
    ModelManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    // 入力の初期化（共通なのでGameに残す）
    input_ = std::make_unique<Input>();
    input_->Initialize(winApp_.get());

    sound_ = std::make_unique<SoundManager>();
    bool ok = sound_->Initialize();
    assert(ok);
    // Game::Initialize() の後半
    offscreenRenderer_ = std::make_unique<OffscreenRenderer>();
    offscreenRenderer_->Initialize(dxCommon_.get(), srvManager_.get());
     // ========== 改善後 ==========

     // 1. 各種ポインタを SceneContext にまとめる
    SceneContext context;
    context.dxCommon = dxCommon_.get();
    context.srvManager = srvManager_.get();
    context.spriteCommon = spriteCommon_.get();
    context.object3dCommon = object3dCommon_.get();
    context.modelCommon = modelCommon_.get();
    context.particleCommon = particleCommon_.get();
    context.camera = camera_.get();
    context.input = input_.get();
    context.sound = sound_.get();
    context.offscreenRenderer = offscreenRenderer_.get();
#ifdef USE_IMGUI
    context.isDebugMode = &isDebugMode_;
#endif

    // 2. SceneManager を作り、Context を「1回だけ」預ける
    sceneManager_ = std::make_unique<SceneManager>();
    sceneManager_->SetContext(context);

    // 3. 最初のシーンをセット（SetContextはSceneManagerが自動でやってくれる！）
    sceneManager_->SetNextScene(std::make_unique<GamePlayScene>());
}

void Game::Update() {
    Framework::Update();

    // メッセージ処理（元 while 内の先頭）
    if (winApp_ && winApp_->ProcessMessage()) {
        endRequest_ = true;
        return;
    }

    if (input_) {
        input_->Update();

        if (input_->TriggerKey(DIK_0)) {
            OutputDebugStringA("push 0\n");
        }
    }

    if (offscreenRenderer_ && input_) {
        offscreenRenderer_->Update(
            1.0f / 60.0f,
            input_->GetMousePosition(),
            input_->PushMouseRight());
    }


#ifdef USE_IMGUI
    // F1を押した瞬間にデバッグモードを切り替える
    if (input_->TriggerKey(DIK_F1)) {
        isDebugMode_ = !isDebugMode_;
    }
#endif


    if (sceneManager_) {
        sceneManager_->Update();
    }
}

void Game::Draw() {
    if (!dxCommon_) { return; }

#ifdef USE_IMGUI
    // ImGuiを初期化している間は毎フレーム開始処理を行う
    if (imguiManager_) {
        imguiManager_->Begin();

        // デバッグモードのときだけ各種デバッグUIを描画する
        if (isDebugMode_ && sceneManager_) {
            sceneManager_->DrawImGui();
        }

        imguiManager_->End();
    }
#endif


    if (offscreenRenderer_) {
        offscreenRenderer_->PreDrawScene();
    }

    if (sceneManager_) {
        sceneManager_->Draw();
    }

    if (offscreenRenderer_) {
        offscreenRenderer_->DrawToBackBuffer();
    }

#ifdef USE_IMGUI
    // ImGuiを実際に描画する
    if (imguiManager_) {
        imguiManager_->Draw();
    }
#endif

    dxCommon_->PostDraw();
}


void Game::Finalize() {

    // シーンを先に落とす
    sceneManager_.reset();

    // ★サウンドをここで確実に止める（再生中でも落ちにくくなる）
    if (sound_) {
        sound_->Finalize();
        sound_.reset();
    }

    ModelManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Finalize();

#ifdef USE_IMGUI
    if (imguiManager_) {
        imguiManager_->Finalize();
    }
    imguiManager_.reset();
#endif

    offscreenRenderer_.reset();

    particleCommon_.reset();
    modelCommon_.reset();
    object3d_.reset();
    object3dCommon_.reset();
    spriteCommon_.reset();
    input_.reset();
    camera_.reset();

    srvManager_.reset();
    dxCommon_.reset();

    if (winApp_) {
        winApp_->Finalize();
    }
    winApp_.reset();

    Framework::Finalize();
}
