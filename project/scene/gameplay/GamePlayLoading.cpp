#include "GamePlayScene.h"
#include "../../engine/base/DirectX/DirectXCommon.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/3d/model/ModelManager.h"
#include "../../engine/particle/Particle.h"
#include <algorithm>
#include <cassert>
#include <thread>
#include <unordered_map>

void GamePlayScene::Initialize()
{
    // 即時初期化の呼び出しにも、分割した仕事と同じ処理を使う。
    BeginInitialize();
    while (!ContinueInitialize()) { std::this_thread::yield(); }
}

void GamePlayScene::BeginInitialize()
{
    if (initialized_ || initializing_) { return; }
    assert(GetContext().dxCommon && GetContext().srvManager && GetContext().spriteCommon);
    assert(GetContext().object3dCommon && GetContext().particleCommon && GetContext().camera);
    assert(GetContext().input && GetSceneManager());
    initializing_ = true;
    loadQueue_.Clear();
    // JSONの解析とNavMeshの接続計算だけを別スレッドへ渡す。GPU操作はここではしない。
    levelFuture_ = std::async(std::launch::async, &GamePlayScene::ReadInitialLevel, levelFilePath_);
    loadQueue_.Add([this] {
        object3d_ = std::make_unique<Object3d>();
        object3d_->Initialize(GetContext().object3dCommon);
    });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("fence.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("plane.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("cube.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("player_modular/Swat.gltf"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("bullet/bullet.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("grenade/grenade.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("enemy/enemy.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("boss/boss.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("block/block.obj"); });
    loadQueue_.Add([] { ModelManager::GetInstance()->LoadModel("casing/casing.obj"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/monsterBall.png"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/checkerBoard.png"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/circle2.png"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/fence.png"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/Cube.png"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/skybox.dds"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/gradationLine.png"); });
    loadQueue_.Add([] { TextureManager::GetInstance()->LoadTexture("Resources/white2x2.png"); });
    loadQueue_.Add([this] {
        materialResource_ = GetContext().dxCommon->CreateBufferResource(sizeof(Material));
        Material* materialData = nullptr;
        materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
        materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        materialData->lightingType = 0;
        materialData->environmentCoefficient = 0.0f;
        materialData->uvTransform = MakeIdentity4x4();
    
        directionalLightResource_ = GetContext().dxCommon->CreateBufferResource(sizeof(DirectionalLight));
        DirectionalLight* light = nullptr;
        directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
        light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        light->direction = Vector3(0.0f, -1.0f, 0.0f);
        light->intensity = 4.0f;
    });
    loadQueue_.Add([this] {
        InitializeGeneratorUi();
    });
    loadQueue_.Add([this] {
        InitializeGuideUi();
    });
    loadQueue_.Add([this] {
        InitializePauseUi();
    });
    loadQueue_.Add([this] {
        // 近接攻撃可能な敵の頭上へ表示するマークを作る
        meleeMarker_ = std::make_unique<Sprite>();
        meleeMarker_->Initialize(
            GetContext().spriteCommon,
            directionalLightResource_.Get(),
            "Resources/circle2.png");
    });
    loadQueue_.Add([this] {
        // ミニマップを初期化する
        minimap_ = std::make_unique<Minimap>();
        minimap_->Initialize(
            GetContext().spriteCommon,
            directionalLightResource_.Get(),
            "Resources/level/minimap.json");
    });
    loadQueue_.Add([this] {
        // マークの中心が敵の頭上に合うように設定する
        meleeMarker_->SetAnchorPoint({ 0.5f, 0.5f });
        meleeMarker_->SetSize({ 42.0f, 42.0f });
        meleeMarker_->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f });
        meleeMarker_->Update();
    });
    loadQueue_.Add([this] {
        // 残弾UI用に最大30個分の黄色い四角Spriteを作っておく
        ammoSprites_.clear();
    });
    for (int index = 0; index < 30; ++index){
        loadQueue_.Add([this] {
            auto ammoSprite = std::make_unique<Sprite>();
            ammoSprite->Initialize(
                GetContext().spriteCommon,
                directionalLightResource_.Get(),
                "Resources/white2x2.png");
            ammoSprite->SetAnchorPoint({ 0.0f, 0.5f });
            ammoSprite->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f });
            ammoSprite->Update();
            ammoSprites_.push_back(std::move(ammoSprite));
        });
    }
    loadQueue_.Add([this] {
        // 残弾UIの後ろに敷く黒い半透明パネルを作っておく
        ammoBackgroundSprite_ = std::make_unique<Sprite>();
        ammoBackgroundSprite_->Initialize(
            GetContext().spriteCommon,
            directionalLightResource_.Get(),
            "Resources/white2x2.png");
        ammoBackgroundSprite_->SetAnchorPoint({ 0.0f, 0.0f });
        ammoBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.35f });
        ammoBackgroundSprite_->Update();
    });
    loadQueue_.Add([this] {
        // 発射演出用のSpriteを作っておく(中心を基準に動かす)
        ammoFireEffectSprites_.clear();
    });
    for (int index = 0; index < 8; ++index){
        loadQueue_.Add([this] {
            auto effectSprite = std::make_unique<Sprite>();
            effectSprite->Initialize(
                GetContext().spriteCommon,
                directionalLightResource_.Get(),
                "Resources/white2x2.png");
            effectSprite->SetAnchorPoint({ 0.5f, 0.5f });
            effectSprite->SetColor({ 1.0f, 0.9f, 0.35f, 0.0f });
            effectSprite->Update();
            ammoFireEffectSprites_.push_back(std::move(effectSprite));
        });
    }
    loadQueue_.Add([this] {
        ammoFireEffects_.assign(ammoFireEffectSprites_.size(), AmmoFireEffect{});
        ammoBulletAnims_.assign(ammoSprites_.size(), AmmoBulletAnim{});
        ammoUiPrevAmmo_ = -1;
        ammoUiPrevMaxAmmo_ = -1;
        ammoUiPrevMode_ = Player::AttackMode::Knife;
    });
    loadQueue_.Add([this] {
        // ボスHPバーの背景を作る
        bossHpBackgroundSprite_ = std::make_unique<Sprite>();
        bossHpBackgroundSprite_->Initialize(
            GetContext().spriteCommon,
            directionalLightResource_.Get(),
            "Resources/white2x2.png");
        bossHpBackgroundSprite_->SetAnchorPoint({ 0.5f, 0.0f });
        bossHpBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        bossHpBackgroundSprite_->Update();
    });
    loadQueue_.Add([this] {
        // ボスHPバーの残量を作る
        bossHpFillSprite_ = std::make_unique<Sprite>();
        bossHpFillSprite_->Initialize(
            GetContext().spriteCommon,
            directionalLightResource_.Get(),
            "Resources/white2x2.png");
        bossHpFillSprite_->SetAnchorPoint({ 0.0f, 0.0f });
        bossHpFillSprite_->SetColor({ 0.95f, 0.05f, 0.03f, 0.0f });
        bossHpFillSprite_->Update();
    });
    loadQueue_.Add([this] {
        // ボスHPバーの外枠を作る
        bossHpFrameSprite_ = std::make_unique<Sprite>();
        bossHpFrameSprite_->Initialize(
            GetContext().spriteCommon,
            directionalLightResource_.Get(),
            "Resources/white2x2.png");
        bossHpFrameSprite_->SetAnchorPoint({ 0.5f, 0.0f });
        bossHpFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        bossHpFrameSprite_->Update();
    });
    loadQueue_.Add([this] {
        // タイトルの暗転から、上下へ開くシャッターで本編を見せる
        introFrame_ = 0;
    });
    for (size_t i = 0; i < introDoors_.size(); ++i){
        loadQueue_.Add([this, i] {
            introDoors_[i] = std::make_unique<Sprite>();
            introDoors_[i]->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/white2x2.png");
            // 上の扉は下端、下の扉は上端を合わせ目に合わせる
            introDoors_[i]->SetAnchorPoint({ 0.0f, i == 0 ? 1.0f : 0.0f });
            introDoors_[i]->SetSize({ static_cast<float>(WinApp::kClientWidth), WinApp::kClientHeight * 0.5f });
            introDoors_[i]->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
            introDoorEdges_[i] = std::make_unique<Sprite>();
            introDoorEdges_[i]->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/white2x2.png");
            introDoorEdges_[i]->SetAnchorPoint({ 0.5f, i == 0 ? 1.0f : 0.0f });
            introDoorGlows_[i] = std::make_unique<Sprite>();
            introDoorGlows_[i]->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/white2x2.png");
            introDoorGlows_[i]->SetAnchorPoint({ 0.5f, 0.5f });
        });
    }
    loadQueue_.Add([this] {
        introFlash_ = std::make_unique<Sprite>();
        introFlash_->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/white2x2.png");
        introFlash_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
    });
    loadQueue_.Add([this] {
        introScan_.Initialize(GetContext().spriteCommon, directionalLightResource_.Get());
    });
    loadQueue_.Add([this] {
        // 作戦開始の文字と赤い線は先に作り、演出中のリソース生成を避ける。
        missionStartLabel_ = std::make_unique<Sprite>();
        missionStartLabel_->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/hud/mission_start.png");
        missionStartLabel_->SetAnchorPoint({ 0.5f, 0.5f });
        missionStartLabel_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        missionStartLabel_->Update();
        missionStartLine_ = std::make_unique<Sprite>();
        missionStartLine_->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/white2x2.png");
        missionStartLine_->SetAnchorPoint({ 0.5f, 0.5f });
        missionStartLine_->SetColor({ 0.70f, 0.12f, 0.12f, 0.0f });
        missionStartLine_->Update();
    });
    loadQueue_.Add([this] {
        UpdateIntroTransition();
    });
    loadQueue_.Add([this] {
        skyboxCommon_ = std::make_unique<SkyboxCommon>();
        skyboxCommon_->Initialize(GetContext().dxCommon, GetContext().srvManager);
        skyboxCommon_->SetDefaultCamera(GetContext().camera);
    });
    loadQueue_.Add([this] {
        skybox_ = std::make_unique<Skybox>();
        skybox_->Initialize(skyboxCommon_.get());
        skybox_->SetCamera(GetContext().camera);
    });
    loadQueue_.Add([this] {
        // 敵の視界範囲をデバッグ線で描画するための準備をする
        // 起動時に作成した共通設定を借り、遷移中のシェーダー再コンパイルを省く。
        line3dCommon_ = GetContext().line3dCommon;
        assert(line3dCommon_);
        enemyVisionDebug_.Initialize(GetContext().dxCommon, line3dCommon_);
    
        debugCamera_ = std::make_unique<DebugCamera>();
    });
    loadQueue_.Add([this] {
        // 全プレイヤー弾で共有する軌跡用パーティクルを作る
        particleSystem_ = std::make_unique<ParticleSystem>();
        particleSystem_->Initialize(
            GetContext().dxCommon,
            GetContext().particleCommon,
            GetContext().camera,
            GetContext().srvManager,
            ParticleType::CircleBurst);
    });
    loadQueue_.Add([this] {
        // 血しぶきは発光しない通常アルファ合成で描画する
        bloodParticleSystem_ = std::make_unique<ParticleSystem>();
        bloodParticleSystem_->Initialize(
            GetContext().dxCommon,
            GetContext().particleCommon,
            GetContext().camera,
            GetContext().srvManager,
            ParticleType::CircleBurst);
        bloodParticleSystem_->SetBlendMode(ParticleBlendMode::Alpha);
    });
    loadQueue_.Add([this] {
        // 爆発後の煙は発光させず、通常アルファ合成で長く残す
        grenadeSmokeParticleSystem_ = std::make_unique<ParticleSystem>();
        grenadeSmokeParticleSystem_->Initialize(
            GetContext().dxCommon,
            GetContext().particleCommon,
            GetContext().camera,
            GetContext().srvManager,
            ParticleType::Smoke);
        grenadeSmokeParticleSystem_->SetBlendMode(ParticleBlendMode::Alpha);
    });
    loadQueue_.Add([this] {
        player_ = std::make_unique<Player>();
        player_->BeginInitialize(GetContext().object3dCommon, GetContext().input, particleSystem_.get());
    });
    // モーションも1段階ずつ作り、読み込み待ちで描画を止めない。
    loadQueue_.AddPoll([this] { return player_->ContinueInitialize(); });
    loadQueue_.Add([this] {
        // グレネード爆発後の煙用パーティクルをPlayerへ渡す
        player_->SetGrenadeSmokeParticleSystem(grenadeSmokeParticleSystem_.get());
    
        followCamera_ = std::make_unique<FollowCamera>();
        followCamera_->Initialize(GetContext().camera);
        followCamera_->SetTarget(player_->GetWorldPosition());
    });
    loadQueue_.Add([this] {
        mapField_.LoadFromCsv("Resources/map.csv");
        player_->SetMap(&mapField_, tileSize_);
    });
    loadQueue_.AddPoll([this] {
        if (levelFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { return false; }
        preparedLevel_ = levelFuture_.get();
        navMeshData_ = std::move(preparedLevel_.level.navMesh);
        ApplyPlayerSpawnFromLevelData(preparedLevel_.objects);
        // JSONが完成してから、設置物と敵の生成を1個ずつ仕事へ追加する。
        QueueInitialLevelObjects();
        return true;
    });
}

bool GamePlayScene::ContinueInitialize()
{
    if (initialized_) { return true; }
    if (!initializing_) { BeginInitialize(); }
    // 重い準備はLoading画面で進める。タイトルの画面破壊演出中には呼び出さない。
    return loadQueue_.Tick();
}

void GamePlayScene::OnActivated(float transitionFrame)
{
    // 直前のタイトル演出の時刻を受け取り、準備完了後に初めて扉を開く。
    introScanFrameOffset_ = transitionFrame;
    introFrame_ = 0;
    generatorPreviousUpdate_ = std::chrono::steady_clock::now();
    player_->SuppressFireUntilRelease();
    followCamera_->SetTarget(player_->GetWorldPosition());
    followCamera_->Update();
    // 通常の追従位置を終点として保存し、同じ角度のままプレイヤーへ寄る。
    missionCameraTo_ = GetContext().camera->GetTranslate();
    missionCameraRotate_ = GetContext().camera->GetRotate();
    const Matrix4x4 cameraRotation = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, missionCameraRotate_, { 0.0f, 0.0f, 0.0f });
    const Vector3 position = player_->GetWorldPosition();
    const Vector3 focus{ position.x, position.y + 0.7f, position.z };
    const Vector3 forward{ cameraRotation.m[2][0], cameraRotation.m[2][1], cameraRotation.m[2][2] };
    missionCameraFrom_ = { focus.x - forward.x * 10.0f,
        focus.y - forward.y * 10.0f, focus.z - forward.z * 10.0f };
    GetContext().camera->SetTranslate(missionCameraFrom_);
    GetContext().camera->Update();
    missionStartFrame_ = 0;
    missionStartActive_ = true;
    if (GetContext().offscreenRenderer) {
        GetContext().offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        GetContext().offscreenRenderer->SetPostEffectEnabled(PostEffectType::Bloom, true);
        GetContext().offscreenRenderer->StopShockwave();
        GetContext().offscreenRenderer->SetShockwaveMaxRadius(0.10f);
    }
}

void GamePlayScene::QueueInitialLevelObjects()
{
    ResetMapObjects();
    loadingMinimapObjects_.clear();
    for (size_t i = 0; i < preparedLevel_.objects.size(); ++i) {
        loadQueue_.Add([this, i] { CreateMapObject(preparedLevel_.objects[i], loadingMinimapObjects_); });
    }
    loadQueue_.Add([this] {
        minimap_->SetObjectMarkers(loadingMinimapObjects_);
        player_->SetFloorColliders(&floorColliders_);
        player_->SetWallColliders(&wallColliders_);
        player_->Respawn();
    });
    // 敵の配置と巡回地点を集めるだけにし、描画リソースの生成は後続の仕事で行う。
    std::unordered_map<std::string, Vector3> spawns;
    std::unordered_map<std::string, std::vector<std::pair<int, Vector3>>> routes;
    for (const auto& object : preparedLevel_.objects) {
        const auto waypoint = object.name.find("_Waypoint_");
        if (waypoint != std::string::npos) {
            routes[object.name.substr(0, waypoint)].push_back({
                std::stoi(object.name.substr(waypoint+std::string("_Waypoint_").size())), object.translation });
        } else if ((object.name.rfind("Enemy_", 0) == 0 || object.name.rfind("enemy_", 0) == 0)
            && object.fileName != "boss/boss.obj") {
            spawns[object.name] = object.translation;
        }
    }
    for (const auto& [name, position] : spawns) {
        auto& sorted = routes[name];
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<Vector3> waypoints;
        for (const auto& point : sorted) { waypoints.push_back(point.second); }
        loadQueue_.Add([this, position, waypoints = std::move(waypoints)] {
            auto enemy = std::make_unique<Enemy>();
            enemy->Initialize(GetContext().object3dCommon, GetContext().camera, position);
            enemy->SetBloodParticleSystem(bloodParticleSystem_.get());
            enemy->SetMap(&mapField_, tileSize_);
            enemy->SetFloorColliders(&floorColliders_);
            enemy->SetWallColliders(&wallColliders_);
            enemy->SetNavMesh(&navMeshData_);
            enemy->SetWaypoints(waypoints);
            enemies_.push_back(std::move(enemy));
        });
    }
    for (size_t i = 0; i < preparedLevel_.objects.size(); ++i) {
        if (preparedLevel_.objects[i].fileName != "boss/boss.obj") { continue; }
        loadQueue_.Add([this, i] {
            const auto& object = preparedLevel_.objects[i];
            auto boss = std::make_unique<Boss>();
            boss->Initialize(GetContext().object3dCommon, GetContext().camera,
                object.translation, object.rotation, object.scaling);
            boss->SetWallColliders(&wallColliders_);
            bosses_.push_back(std::move(boss));
        });
    }
    loadQueue_.Add([this] {
        // すべての生成が完了した状態だけを、SceneManagerへ公開する。
        levelHotReload_.Initialize(levelFilePath_);
        reloadNoticeText_ = "Level loaded";
        reloadNoticeFrameCount_ = 180;
        preparedLevel_ = PreparedLevel{};
        loadingMinimapObjects_.clear();
        generatorPreviousUpdate_ = std::chrono::steady_clock::now();
        initializing_ = false;
        initialized_ = true;
    });
}
