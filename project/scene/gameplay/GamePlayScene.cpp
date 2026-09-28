#include "GamePlayScene.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <memory>
#include <cmath>
#include <random>
#include <unordered_map>
#include <cstring>
#include <cstdint>

#include "../engine/base/directX/DirectXCommon.h"
#include "../engine/base/winapp/WinApp.h"
#include "../engine/base/srv/SrvManager.h"
#include "../engine/2d/sprite/SpriteCommon.h"
#include "../engine/2d/sprite/Sprite.h"

#include "../engine/3d/obj3d/Object3dCommon.h"
#include "../engine/3d/obj3d/Object3d.h"
#include "../game/camera/Camera.h"
#include "../../game/player/PlayerBullet.h"

#include "../engine/particle/ParticleCommon.h"
#include "../engine/particle/Particle.h"
#include "../engine/3d/model/ModelCommon.h"
#include "../engine/3d/model/ModelManager.h"
#include "../engine/2d/texture/TextureManager.h"

#include "../engine/3d/skybox/SkyboxCommon.h"
#include "../engine/3d/skybox/Skybox.h"

#include "../engine/base/offscreen/OffscreenRenderer.h"

#include "../engine/input/Input.h"
#include "../scene/SceneManager.h"

#include "../clear/ClearScene.h"
#include "../gameover/GameOverScene.h"

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

namespace {

    // プレイヤーの当たり判定から発電機の表面まで届く操作距離
    constexpr float kGeneratorInteractionRange = 2.0f;
    // 起動が完了した後、満タンのメーターを表示しておく秒数
    constexpr double kGeneratorCompleteNoticeSeconds = 3.0;

    // 発電機の音を聞き取れる距離と、プレイヤーに聞こえる最大音量
    constexpr float kGeneratorSoundRadius = 25.0f;
    constexpr float kGeneratorMotorVolume = 0.35f;

    // 1秒の周期がつながる仮モーター音を作る。後から録音した音へ差し替え可能
    SoundData MakeGeneratorMotorSound()
    {
        SoundData sound;
        sound.wfex.wFormatTag = WAVE_FORMAT_PCM;
        sound.wfex.nChannels = 1;
        sound.wfex.nSamplesPerSec = 22050;
        sound.wfex.wBitsPerSample = 16;
        sound.wfex.nBlockAlign = 2;
        sound.wfex.nAvgBytesPerSec = sound.wfex.nSamplesPerSec * sound.wfex.nBlockAlign;
        sound.buffer.resize(sound.wfex.nAvgBytesPerSec);
        for (uint32_t index = 0; index < sound.wfex.nSamplesPerSec; ++index) {
            const double t = static_cast<double>(index) / sound.wfex.nSamplesPerSec;
            const double phase = t * 6.283185307179586;
            const double pulse = 0.75 + 0.25 * std::sin(phase * 12.0);
            const double wave = (std::sin(phase * 60.0) + 0.45 * std::sin(phase * 120.0) +
                0.2 * std::sin(phase * 240.0)) * pulse;
            const int16_t sample = static_cast<int16_t>(wave * 7000.0);
            std::memcpy(sound.buffer.data() + index * sizeof(sample), &sample, sizeof(sample));
        }
        return sound;
    }

    // レベルオブジェクトの階層をワールド座標の一覧に変換する
    void FlattenLevelObjectsRecursive(
        const LevelObjectData& objectData,
        const Vector3& parentTranslation,
        std::vector<LevelObjectData>& outObjects)
    {
        // 元の階層データを壊さないようにコピーして扱う
        LevelObjectData worldObject = objectData;

        // 親の座標を足して、このノードをワールド座標にする
        worldObject.translation.x += parentTranslation.x;
        worldObject.translation.y += parentTranslation.y;
        worldObject.translation.z += parentTranslation.z;

        // 平坦化後は1つのオブジェクトとして扱うため、子要素は消す
        worldObject.children.clear();

        // 平坦化したオブジェクトを一覧に追加する
        outObjects.push_back(worldObject);

        // このオブジェクトの座標を親座標として、子要素も再帰的に平坦化する
        for (const LevelObjectData& child : objectData.children) {
            FlattenLevelObjectsRecursive(child, worldObject.translation, outObjects);
        }
    }

    // レベル内の全オブジェクトをワールド座標の一覧として返す
    std::vector<LevelObjectData> FlattenAllLevelObjects(const LevelData& levelData)
    {
        std::vector<LevelObjectData> allObjects;

        for (const LevelObjectData& objectData : levelData.objects) {
            FlattenLevelObjectsRecursive(objectData, Vector3{ 0.0f, 0.0f, 0.0f }, allObjects);
        }

        return allObjects;
    }

    // 球と箱の当たり判定を調べる
    bool IsSphereHitBox(const SphereCollider& sphere, const Vector3& boxCenter, const Vector3& boxSize)
    {
        const Vector3 halfSize = {
            boxSize.x * 0.5f,
            boxSize.y * 0.5f,
            boxSize.z * 0.5f
        };

        const float closestX = std::clamp(sphere.center.x, boxCenter.x - halfSize.x, boxCenter.x + halfSize.x);
        const float closestY = std::clamp(sphere.center.y, boxCenter.y - halfSize.y, boxCenter.y + halfSize.y);
        const float closestZ = std::clamp(sphere.center.z, boxCenter.z - halfSize.z, boxCenter.z + halfSize.z);

        const float dx = sphere.center.x - closestX;
        const float dy = sphere.center.y - closestY;
        const float dz = sphere.center.z - closestZ;

        return dx * dx + dy * dy + dz * dz <= sphere.radius * sphere.radius;
    }


    bool TryConvertWorldToScreenUV(
        const Vector3& worldPosition,
        const Matrix4x4& viewProjectionMatrix,
        Vector2& outUV)
    {
        // ワールド座標をクリップ座標へ変換する
        const float clipX =
            worldPosition.x * viewProjectionMatrix.m[0][0] +
            worldPosition.y * viewProjectionMatrix.m[1][0] +
            worldPosition.z * viewProjectionMatrix.m[2][0] +
            viewProjectionMatrix.m[3][0];
        const float clipY =
            worldPosition.x * viewProjectionMatrix.m[0][1] +
            worldPosition.y * viewProjectionMatrix.m[1][1] +
            worldPosition.z * viewProjectionMatrix.m[2][1] +
            viewProjectionMatrix.m[3][1];
        const float clipW =
            worldPosition.x * viewProjectionMatrix.m[0][3] +
            worldPosition.y * viewProjectionMatrix.m[1][3] +
            worldPosition.z * viewProjectionMatrix.m[2][3] +
            viewProjectionMatrix.m[3][3];

        // カメラの後ろにある位置は画面UVにできない
        if (clipW <= 0.0f) {
            return false;
        }

        const float ndcX = clipX / clipW;
        const float ndcY = clipY / clipW;

        // NDC座標をポストエフェクト用の0.0fから1.0fのUVへ変換する
        outUV.x = (ndcX + 1.0f) * 0.5f;
        outUV.y = (1.0f - ndcY) * 0.5f;

        return outUV.x >= 0.0f && outUV.x <= 1.0f && outUV.y >= 0.0f && outUV.y <= 1.0f;
    }

    // 残弾UIの並び方をまとめた情報
    struct AmmoUiLayout {
        Vector2 bulletSize{};
        float gapX = 0.0f;
        float gapY = 0.0f;
        int bulletsPerRow = 1;
        float startX = 0.0f;
        float startY = 0.0f;
    };

    // 弾番号(0が最後に撃つ弾)と現在の残弾数から描画位置を求める
    // 下段(一段目)が先に消費され、撃つたびに下段の弾は出口(右端)へ詰まる
    // 上段(2段目)は列を保ったまま待機し、下段を撃ち切ると真下へ落ちて下段になる
    Vector2 GetAmmoBulletPosition(
        const AmmoUiLayout& layout, int currentAmmo, int bulletIndex)
    {
        // 下段の弾数を求める(あふれた分だけ上段が満杯で残る)
        int bottomCount = currentAmmo;
        int topCount = 0;
        if (currentAmmo > layout.bulletsPerRow) {
            topCount = layout.bulletsPerRow;
            bottomCount = currentAmmo - layout.bulletsPerRow;
        }

        int row = 0;
        int column = 0;
        if (bulletIndex < topCount) {
            // 上段は左端から列を固定して並ぶ
            row = 1;
            column = bulletIndex;
        } else {
            // 下段は出口(右端)へ右詰めで並ぶ
            row = 0;
            column = layout.bulletsPerRow - bottomCount + (bulletIndex - topCount);
        }

        return {
            layout.startX + static_cast<float>(column) * (layout.bulletSize.x + layout.gapX),
            layout.startY - static_cast<float>(row) * (layout.bulletSize.y + layout.gapY)
        };
    }
}

void GamePlayScene::ApplyPlayerSpawnFromLevelData(const LevelData& levelData)
{
    // Flatten the hierarchy so the Player object can be found by name.
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        // Use the Player object as the spawn position source.
        if (objectData.objectKind == "player" || objectData.name == "Player") {
            if (player_) {
                player_->SetSpawnPosition(objectData.translation);
            }
            return;
        }
    }
}

bool GamePlayScene::CheckBossTeleport()
{
    if (!player_) {
        return false;
    }

    const SphereCollider playerCollider = player_->GetCollider();

    for (const BossTeleportData& teleport : bossTeleports_) {
        if (teleport.targetLevel.empty()) {
            continue;
        }

        if (!IsSphereHitBox(playerCollider, teleport.center, teleport.size)) {
            continue;
        }

        // テレポーターを踏んだらボスステージへ移動する
        constexpr bool kTeleportGoesToClear = false;
        if (kTeleportGoesToClear) {
            sceneManager_->SetNextScene(std::make_unique<ClearScene>());
            return true;
        }

        // 指定されたJSONを監視対象にしてレベルを読み直す
        levelFilePath_ = teleport.targetLevel;
        levelHotReload_.Initialize(levelFilePath_);
        ReloadLevel(false);

        reloadNoticeText_ = "Teleported to " + levelFilePath_;
        reloadNoticeFrameCount_ = 180;
        return true;
    }

    return false;
}

void GamePlayScene::ReloadLevel(bool isManualReload)
{
    // Load the latest level JSON file.
    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);

    // Update the player spawn position from the level data.
    ApplyPlayerSpawnFromLevelData(levelData);

    // Rebuild walls, floors, and colliders from the current level data.
    CreateMapObjects();

    // Refresh the player collider references and return to the spawn position.
    if (player_) {
        player_->SetFloorColliders(&floorColliders_);
        player_->SetWallColliders(&wallColliders_);
        player_->Respawn();
    }

    // Recreate enemies from the current level data.
    SpawnEnemies();
    SpawnBosses();

    // Sync the current file timestamp after a successful reload.
    levelHotReload_.SyncCurrentWriteTime();

    // Show whether the reload came from F5 or automatic file watching.
    if (isManualReload) {
        reloadNoticeText_ = "Level reloaded by F5";
    } else {
        reloadNoticeText_ = "Level reloaded automatically";
    }

    reloadNoticeFrameCount_ = 180;
}

void GamePlayScene::Initialize()
{
    if (initialized_) { return; }
    initialized_ = true;

    assert(context_.dxCommon);
    assert(context_.srvManager);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.modelCommon);
    assert(context_.particleCommon);
    assert(context_.camera);
    assert(context_.input);
    assert(context_.sound);
    assert(sceneManager_);

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(context_.object3dCommon);

    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("cube.obj");
    ModelManager::GetInstance()->LoadModel("player/player.obj");
    ModelManager::GetInstance()->LoadModel("bullet/bullet.obj");
    // Gキーで投げるグレネードモデルを読み込む
    ModelManager::GetInstance()->LoadModel("grenade/grenade.obj");
    ModelManager::GetInstance()->LoadModel("enemy/enemy.obj");
    ModelManager::GetInstance()->LoadModel("boss/boss.obj");
    ModelManager::GetInstance()->LoadModel("block/block.obj");

    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle2.png");
    texMan->LoadTexture("Resources/fence.png");
    texMan->LoadTexture("Resources/Cube.png");
    texMan->LoadTexture("Resources/skybox.dds");
    texMan->LoadTexture("Resources/gradationLine.png");
    texMan->LoadTexture("Resources/white2x2.png");

    materialResource_ = context_.dxCommon->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->lightingType = 0;
    materialData->environmentCoefficient = 0.0f;
    materialData->uvTransform = MakeIdentity4x4();

    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

    // 発電機用の操作案内と起動メーターを準備する
    InitializeGeneratorUi();
    // 通常のSpriteで案内を作り、Releaseでも同じ説明を表示する
    InitializeGuideUi();
    generatorMotorSound_ = MakeGeneratorMotorSound();

    // 近接攻撃可能な敵の頭上へ表示するマークを作る
    meleeMarker_ = std::make_unique<Sprite>();
    meleeMarker_->Initialize(
        context_.spriteCommon,
        directionalLightResource_.Get(),
        "Resources/circle2.png");

    // ミニマップを初期化する
    minimap_ = std::make_unique<Minimap>();
    minimap_->Initialize(
        context_.spriteCommon,
        directionalLightResource_.Get(),
        "Resources/level/minimap.json");

    // マークの中心が敵の頭上に合うように設定する
    meleeMarker_->SetAnchorPoint({ 0.5f, 0.5f });
    meleeMarker_->SetSize({ 42.0f, 42.0f });
    meleeMarker_->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f });
    meleeMarker_->Update();
    // 残弾UI用に最大30個分の黄色い四角Spriteを作っておく
    ammoSprites_.clear();
    for (int index = 0; index < 30; ++index) {
        auto ammoSprite = std::make_unique<Sprite>();
        ammoSprite->Initialize(
            context_.spriteCommon,
            directionalLightResource_.Get(),
            "Resources/white2x2.png");
        ammoSprite->SetAnchorPoint({ 0.0f, 0.5f });
        ammoSprite->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f });
        ammoSprite->Update();
        ammoSprites_.push_back(std::move(ammoSprite));
    }
    // 残弾UIの後ろに敷く黒い半透明パネルを作っておく
    ammoBackgroundSprite_ = std::make_unique<Sprite>();
    ammoBackgroundSprite_->Initialize(
        context_.spriteCommon,
        directionalLightResource_.Get(),
        "Resources/white2x2.png");
    ammoBackgroundSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    ammoBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.35f });
    ammoBackgroundSprite_->Update();
    // 発射演出用のSpriteを作っておく(中心を基準に動かす)
    ammoFireEffectSprites_.clear();
    for (int index = 0; index < 8; ++index) {
        auto effectSprite = std::make_unique<Sprite>();
        effectSprite->Initialize(
            context_.spriteCommon,
            directionalLightResource_.Get(),
            "Resources/white2x2.png");
        effectSprite->SetAnchorPoint({ 0.5f, 0.5f });
        effectSprite->SetColor({ 1.0f, 0.9f, 0.35f, 0.0f });
        effectSprite->Update();
        ammoFireEffectSprites_.push_back(std::move(effectSprite));
    }
    ammoFireEffects_.assign(ammoFireEffectSprites_.size(), AmmoFireEffect{});
    ammoBulletAnims_.assign(ammoSprites_.size(), AmmoBulletAnim{});
    ammoUiPrevAmmo_ = -1;
    ammoUiPrevMaxAmmo_ = -1;
    ammoUiPrevMode_ = Player::AttackMode::Knife;

    // ボスHPバーの背景を作る
    bossHpBackgroundSprite_ = std::make_unique<Sprite>();
    bossHpBackgroundSprite_->Initialize(
        context_.spriteCommon,
        directionalLightResource_.Get(),
        "Resources/white2x2.png");
    bossHpBackgroundSprite_->SetAnchorPoint({ 0.5f, 0.0f });
    bossHpBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
    bossHpBackgroundSprite_->Update();

    // ボスHPバーの残量を作る
    bossHpFillSprite_ = std::make_unique<Sprite>();
    bossHpFillSprite_->Initialize(
        context_.spriteCommon,
        directionalLightResource_.Get(),
        "Resources/white2x2.png");
    bossHpFillSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    bossHpFillSprite_->SetColor({ 0.95f, 0.05f, 0.03f, 0.0f });
    bossHpFillSprite_->Update();

    // ボスHPバーの外枠を作る
    bossHpFrameSprite_ = std::make_unique<Sprite>();
    bossHpFrameSprite_->Initialize(
        context_.spriteCommon,
        directionalLightResource_.Get(),
        "Resources/white2x2.png");
    bossHpFrameSprite_->SetAnchorPoint({ 0.5f, 0.0f });
    bossHpFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    bossHpFrameSprite_->Update();

    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(context_.dxCommon, context_.srvManager);
    skyboxCommon_->SetDefaultCamera(context_.camera);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(skyboxCommon_.get());
    skybox_->SetCamera(context_.camera);

    // 敵の視界範囲をデバッグ線で描画するための準備をする
    line3dCommon_ = std::make_unique<Line3DCommon>();
    line3dCommon_->Initialize(context_.dxCommon, context_.srvManager);
    enemyVisionDebug_.Initialize(context_.dxCommon, line3dCommon_.get());

    debugCamera_ = std::make_unique<DebugCamera>();

    // 全プレイヤー弾で共有する軌跡用パーティクルを作る
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::CircleBurst);

    // 血しぶきは発光しない通常アルファ合成で描画する
    bloodParticleSystem_ = std::make_unique<ParticleSystem>();
    bloodParticleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::CircleBurst);
    bloodParticleSystem_->SetBlendMode(ParticleBlendMode::Alpha);

    // 爆発後の煙は発光させず、通常アルファ合成で長く残す
    grenadeSmokeParticleSystem_ = std::make_unique<ParticleSystem>();
    grenadeSmokeParticleSystem_->Initialize(
        context_.dxCommon,
        context_.particleCommon,
        context_.camera,
        context_.srvManager,
        ParticleType::Smoke);
    grenadeSmokeParticleSystem_->SetBlendMode(ParticleBlendMode::Alpha);

    player_ = std::make_unique<Player>();

    // Player初期化前に、BlenderのレベルデータからPlayer配置を読む
    {
        LevelData levelData = LevelLoader::LoadFile("Resources/level/testScene.json");
        std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

        for (const LevelObjectData& objectData : allObjects) {
            // Blender上のPlayerオブジェクト位置をスポーン位置として使う
            if (objectData.name == "Player") {
                player_->SetSpawnPosition(objectData.translation);
                break;
            }
        }
    }

    player_->Initialize(
        context_.object3dCommon,
        context_.input,
        particleSystem_.get());

    // グレネード爆発後の煙用パーティクルをPlayerへ渡す
    player_->SetGrenadeSmokeParticleSystem(grenadeSmokeParticleSystem_.get());

    followCamera_ = std::make_unique<FollowCamera>();
    followCamera_->Initialize(context_.camera);
    followCamera_->SetTarget(player_->GetWorldPosition());

    if (context_.offscreenRenderer) {
        context_.offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        // タイトルの衝撃波を本編へ持ち越さない
        context_.offscreenRenderer->StopShockwave();
        // ゲームシーン側で通常の衝撃波サイズを調整する
        context_.offscreenRenderer->SetShockwaveMaxRadius(0.10f);
    }

    mapField_.LoadFromCsv("Resources/map.csv");
    player_->SetMap(&mapField_, tileSize_);

    // Initialize hot reload watching for the level JSON file.
    levelHotReload_.Initialize(levelFilePath_);
    // Load the level JSON and rebuild the player spawn, map objects, and enemies.
    ReloadLevel(false);

    // 初期化にかかった時間を発電機の進捗へ加算しない
    generatorPreviousUpdate_ = std::chrono::steady_clock::now();
}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

    // 発電機の15秒は描画フレーム数に依存させず、実際の経過時間で測る
    const auto generatorNow = std::chrono::steady_clock::now();
    const double generatorDeltaSeconds =
        std::chrono::duration<double>(generatorNow - generatorPreviousUpdate_).count();
    generatorPreviousUpdate_ = generatorNow;
    generatorUiIndex_ = -1;

    // Count down the reload notice display time each frame.
    if (reloadNoticeFrameCount_ > 0) {
        reloadNoticeFrameCount_--;
    }

    // Reload the level JSON manually when F5 is pressed.
    if (context_.input && context_.input->TriggerKey(DIK_F5)) {
        ReloadLevel(true);
    }

    // Mキーを押した瞬間にミニマップの大きさを切り替える
    if (context_.input &&
        context_.input->TriggerKey(DIK_M)) {
        if (minimap_) {
            minimap_->ToggleExpanded();
        }
    }

    // Reload the level automatically after the file update becomes stable.
    if (levelHotReload_.Update()) {
        ReloadLevel(false);
    }

    // 死亡したら白黒の演出を少し見せてから、ゲームオーバー画面へ移る
    constexpr int kGameOverDelayFrames = 90;
    if (player_ && player_->IsDead()) {
        if (++playerDeadFrames_ >= kGameOverDelayFrames) {
            sceneManager_->SetNextScene(std::make_unique<GameOverScene>());
            return;
        }
    } else {
        playerDeadFrames_ = 0;
    }

    if (skybox_) { skybox_->Update(); }
    if (particleSystem_) { particleSystem_->Update(dt); }
    if (bloodParticleSystem_) { bloodParticleSystem_->Update(dt); }
    if (grenadeSmokeParticleSystem_) { grenadeSmokeParticleSystem_->Update(dt); }

    if (debugCamera_ && context_.isDebugMode) {
        debugCamera_->Update(
            context_.camera,
            context_.input,
            context_.offscreenRenderer,
            *context_.isDebugMode);
    }

    if (context_.isDebugMode && *context_.isDebugMode) {
        if (context_.camera) {
            context_.camera->Update();
        }

        if (player_) {
            player_->UpdateRenderOnly();
        }

        for (auto& enemy : enemies_) {
            enemy->UpdateRenderOnly();
        }

        for (auto& boss : bosses_) {
            // デバッグ中に攻撃の溜めや撃破演出を進めない
            boss->UpdateRenderOnly();
        }

        for (auto& floorObject : floorObjects_) {
            floorObject->Update();
        }

        for (auto& wallObject : wallObjects_) {
            wallObject->Update();
        }

        if (skybox_) {
            skybox_->Update();
        }

        // デバッグカメラ中は進行を止めている発電機の音も消す
        for (auto& generator : generators_) {
            if (generator.motorSound) {
                generator.motorSound->SetVolume(0.0f);
            }
        }

        // デバッグカメラ中は近接攻撃マークを表示しない
        meleeTarget_ = nullptr;
        return;
    }

    if (player_) {
        player_->Update(context_.camera);

        // Player更新中に発生したグレネード爆発を敵へ反映する
        CheckGrenadeExplosions();
    }
    if (context_.camera) { context_.camera->Update(); }

    if (player_ && followCamera_) {
        // グレネード爆発通知を受け取ったフレームからカメラを揺らす
        if (player_->ConsumeGrenadeShakeRequest()) {
            followCamera_->StartShake();
        }

        followCamera_->SetTarget(player_->GetWorldPosition());
        followCamera_->Update();
    }

    // カメラ更新後の行列で弾の発射位置に画面歪みを出す
    StartBulletShockwaves();

    for (auto& floorObject : floorObjects_) {
        floorObject->Update();
    }

    for (auto& wallObject : wallObjects_) {
        wallObject->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }

    // 発砲したら銃声をゲーム内の「音」として発生させ、範囲内の敵へ伝える
    if (player_ && player_->HasFiredThisFrame()) {
        const Vector3 soundPosition = player_->GetWorldPosition();
        const float soundRange = player_->GetGunshotSoundRange();

        for (const auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            // XZ平面の距離で音が届くかを判定する
            const Vector3 enemyPosition = enemy->GetWorldPosition();
            const float deltaX = enemyPosition.x - soundPosition.x;
            const float deltaZ = enemyPosition.z - soundPosition.z;
            if (deltaX * deltaX + deltaZ * deltaZ <=
                soundRange * soundRange) {
                enemy->OnHearSound(soundPosition);
            }
        }

        // ミニマップの音範囲円を光らせる
        if (minimap_) {
            minimap_->NotifyGunshot();
        }
    }

    for (auto& enemy : enemies_) {
        // 敵の追跡目標を現在のプレイヤー位置に更新する
        enemy->SetTargetPosition(player_->GetWorldPosition());

        // 敵の移動と状態を更新する
        enemy->Update();


        // 敵更新後に重なりを解消する
        ResolveEnemyOverlap();
    }

    for (auto& boss : bosses_) {
        // 巨大ゾンビが生存中のプレイヤーを追跡して攻撃する
        boss->Update(player_ ? player_->GetWorldPosition() : Vector3{},
            player_ && !player_->IsDead());

        // 溜め中は赤い予告円、着地時は外へ広がる土煙色の衝撃を出す
        if (particleSystem_ && (boss->IsWindingUp() || boss->IsImpactFrame())) {
            constexpr int kCirclePoints = 40;
            constexpr float kTwoPi = 6.2831853f;
            const bool impact = boss->IsImpactFrame();
            const Vector3& center = boss->GetAttackCenter();
            for (int i = 0; i < kCirclePoints; ++i) {
                const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kCirclePoints);
                const float x = std::cos(angle);
                const float z = std::sin(angle);
                const Vector3 position{
                    center.x + x * boss->GetAttackRadius(), center.y,
                    center.z + z * boss->GetAttackRadius()
                };
                const Vector3 velocity = impact ? Vector3{ x * 6.0f, 1.5f, z * 6.0f } : Vector3{};
                const Vector4 color = impact ? Vector4{ 1.0f, 0.65f, 0.25f, 0.9f }
                    : Vector4{ 1.0f, 0.08f, 0.03f, 0.8f };
                const float size = impact ? 0.7f : 0.25f;
                particleSystem_->Emit(position, { size, size, size }, velocity, color,
                    impact ? 0.45f : 0.04f);
            }
        }
        if (boss->IsImpactFrame() && followCamera_) {
            // 攻撃が外れても、重い着地の振動をカメラで伝える
            followCamera_->StartShake(0.45f, 18);
        }
    }

    // ボスがいない通常ステージではクリアにしないため、削除前の状態を保存する
    const bool hadBoss = !bosses_.empty();

    bosses_.erase(
        std::remove_if(
            bosses_.begin(),
            bosses_.end(),
            [](const std::unique_ptr<Boss>& boss) {
                // 撃破演出が終わったボスを消す
                return boss->IsReadyToRemove();
            }),
        bosses_.end());

    // 全ボスの撃破演出が終わったらクリア画面へ進む。プレイヤー死亡時はゲームオーバーを優先する
    if (hadBoss && bosses_.empty() && player_ && !player_->IsDead()) {
        sceneManager_->SetNextScene(std::make_unique<ClearScene>());
        return;
    }

    // ミニマップへ渡す生存中の敵位置を集める
    std::vector<Vector3> minimapEnemyPositions;

    for (const auto& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        minimapEnemyPositions.push_back(
            enemy->GetWorldPosition());
    }

    // プレイヤーと敵の現在位置をミニマップへ反映する
    if (minimap_ && player_) {
        // 銃声の届く範囲をミニマップの円表示へ渡す
        minimap_->SetSoundRange(player_->GetGunshotSoundRange());
        minimap_->Update(
            player_->GetWorldPosition(),
            minimapEnemyPositions);
    }

    // 残弾数に合わせて左下の弾UIを更新する
    UpdateAmmoUiSprites();

    // ボスのHPに合わせて画面上部のHPバーを更新する
    UpdateBossHpUiSprites();

    // 全敵の視界判定後に近接攻撃を処理する
    UpdateMeleeAttack();

    CheckCollisions();

    // 移動とダメージ判定の後に、近くの発電機へのE入力を処理する
    UpdateGeneratorInteraction(generatorDeltaSeconds);
    UpdateGeneratorWorld(generatorDeltaSeconds);
    UpdateTeleporterEffects();

    if (player_ && context_.offscreenRenderer) {
        const bool isAimingGun =
            context_.input &&
            context_.input->PushMouseRight() &&
            !player_->IsDead();

        const float targetVignetteIntensity = isAimingGun ? 1.0f : 0.0f;
        const float vignetteFadeSpeed = isAimingGun
            ? aimingVignetteFadeInSpeed_
            : aimingVignetteFadeOutSpeed_;

        // 右クリック状態に合わせてビネットを急に切り替えず、少しずつ近づける
        if (aimingVignetteIntensity_ < targetVignetteIntensity) {
            aimingVignetteIntensity_ = std::min(
                aimingVignetteIntensity_ + vignetteFadeSpeed,
                targetVignetteIntensity);
        } else if (aimingVignetteIntensity_ > targetVignetteIntensity) {
            aimingVignetteIntensity_ = std::max(
                aimingVignetteIntensity_ - vignetteFadeSpeed,
                targetVignetteIntensity);
        }

        context_.offscreenRenderer->SetVignetteIntensity(aimingVignetteIntensity_);
        context_.offscreenRenderer->SetPostEffectEnabled(
            PostEffectType::Vignette,
            aimingVignetteIntensity_ > 0.0f);

        if (player_->IsDead()) {
            aimingVignetteIntensity_ = 0.0f;
            context_.offscreenRenderer->SetVignetteIntensity(0.0f);
            context_.offscreenRenderer->SetPostEffectEnabled(PostEffectType::Vignette, false);
            context_.offscreenRenderer->SetPostEffectType(PostEffectType::Grayscale);
        } else if (context_.offscreenRenderer->GetPostEffectType() != PostEffectType::Shockwave) {
            // 衝撃波の再生中はCopyへ戻さず、OffscreenRenderer側の終了処理に任せる
            context_.offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        }
    }
}

void GamePlayScene::Draw()
{
    assert(context_.dxCommon);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.particleCommon);

    ID3D12GraphicsCommandList* commandList = context_.dxCommon->GetCommandList();
    assert(commandList);

    context_.spriteCommon->CommonDrawSetting();

    if (skybox_) {
        skybox_->Draw();
    }

    if (player_) {
        player_->Draw();
    }

    for (auto& enemy : enemies_) {
        enemy->Draw();
    }

    for (auto& boss : bosses_) {
        boss->Draw();
    }

    for (auto& floorObject : floorObjects_) {
        floorObject->Draw();
    }

    for (auto& wallObject : wallObjects_) {
        wallObject->Draw();
    }

    // 透明な加算パーティクルは床や壁に上書きされないよう最後に描画する
    // 血しぶきは発光する弾エフェクトより先に描画する
    // 煙を発光エフェクトより先に通常アルファ合成で描画する
    if (grenadeSmokeParticleSystem_) {
        grenadeSmokeParticleSystem_->Draw();
    }

    if (bloodParticleSystem_) {
        bloodParticleSystem_->Draw();
    }

    if (particleSystem_) {
        particleSystem_->Draw();
    }

    // 敵の視界範囲をデバッグ線で描画する
    if (context_.camera && line3dCommon_) {
        enemyVisionDebug_.Reset();

        for (const auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            enemy->AppendVisionDebugLines(enemyVisionDebug_);
        }

        enemyVisionDebug_.SetWVP(MakeIdentity4x4(), context_.camera->GetViewProjectionMatrix());
        enemyVisionDebug_.Upload();
        enemyVisionDebug_.Draw();
    }
    // 近接攻撃可能な敵の頭上へマークを表示する
    if (meleeTarget_ && meleeMarker_ && context_.camera) {
        Vector3 markerPosition = meleeTarget_->GetWorldPosition();
        markerPosition.y += 2.0f;

        const Matrix4x4& viewProjection = context_.camera->GetViewProjectionMatrix();

        // 敵のワールド座標を画面表示用のクリップ座標へ変換する
        const float clipX =
            markerPosition.x * viewProjection.m[0][0] +
            markerPosition.y * viewProjection.m[1][0] +
            markerPosition.z * viewProjection.m[2][0] +
            viewProjection.m[3][0];
        const float clipY =
            markerPosition.x * viewProjection.m[0][1] +
            markerPosition.y * viewProjection.m[1][1] +
            markerPosition.z * viewProjection.m[2][1] +
            viewProjection.m[3][1];
        const float clipW =
            markerPosition.x * viewProjection.m[0][3] +
            markerPosition.y * viewProjection.m[1][3] +
            markerPosition.z * viewProjection.m[2][3] +
            viewProjection.m[3][3];

        // カメラより前にいる敵のマークだけ描画する
        if (clipW > 0.001f) {
            const float screenX =
                ((clipX / clipW) + 1.0f) * 0.5f * WinApp::kClientWidth;
            const float screenY =
                (1.0f - (clipY / clipW)) * 0.5f * WinApp::kClientHeight;

            meleeMarker_->SetPosition({ screenX, screenY });
            meleeMarker_->Update();
            meleeMarker_->Draw();
        }
    }

    // 3Dオブジェクトより手前へミニマップを描画する
    if (minimap_) {
        minimap_->Draw();
    }

    // 画面上部にボスHPバーを描画する
    DrawBossHpUiSprites();

    // 左下に残弾UIを描画する
    DrawAmmoUiSprites();

    // 発電機の操作案内を通常のゲーム画面に重ねる
    DrawGeneratorUi();
    // 戦闘の中心や下中央の起動メーターを避け、画面の端へ案内を置く
    DrawGuideUi();
}

void GamePlayScene::InitializeGuideUi()
{
    // 数字画像は描画前に読み込む。描画中の初回読み込みはコマンドリストをリセットし、
    // 描画先や深度バッファの設定を失うため、数字Spriteの作成時にはキャッシュを使う
    TextureManager::GetInstance()->LoadTexture("Resources/hud/count_glyphs.png");

    // 日本語は事前生成した透過画像を使い、実行環境のフォントに依存させない
    auto createSprite = [this](const char* texture) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(context_.spriteCommon, directionalLightResource_.Get(), texture);
        sprite->SetAnchorPoint({ 0.0f, 0.0f });
        return sprite;
    };
    objectivePanel_ = createSprite("Resources/white2x2.png");
    objectivePanel_->SetPosition({ 20.0f, 88.0f });
    objectivePanel_->SetColor({ 0.02f, 0.025f, 0.03f, 0.78f });
    objectiveLabel_ = createSprite("Resources/hud/objective_labels.png");
    objectiveLabel_->SetPosition({ 36.0f, 94.0f });
    objectiveLabel_->SetTextureSize({ 640.0f, 128.0f });
    objectiveLabel_->SetSize({ 320.0f, 64.0f });
    objectiveCountSprites_.clear();

    // 右上のミニマップ・左下の残弾・下中央の起動メーターと重ならない位置にする
    controlsGuide_ = createSprite("Resources/hud/controls_guide.png");
    controlsGuide_->SetPosition({ static_cast<float>(WinApp::kClientWidth) - 344.0f,
        static_cast<float>(WinApp::kClientHeight) - 252.0f });
    controlsGuide_->SetSize({ 324.0f, 232.0f });
    controlsGuide_->Update();
}

void GamePlayScene::DrawGuideUi()
{
    // 死亡演出中とデバッグカメラ中は、操作できない案内を隠す
    if (!player_ || player_->IsDead() || (context_.isDebugMode && *context_.isDebugMode)) {
        return;
    }
    context_.spriteCommon->CommonDrawSetting();
    controlsGuide_->Draw();

    // 起動途中は数に含めず、メーターが満タンになった発電機だけを数える
    const size_t activeCount = static_cast<size_t>(std::count_if(generators_.begin(), generators_.end(),
        [](const GeneratorData& generator) {
            return generator.activation.GetState() == GeneratorActivation::State::Active;
        }));
    const bool allActive = !generators_.empty() && activeCount == generators_.size();
    const bool showPortal = allActive ||
        (generators_.empty() && !bossTeleports_.empty() && IsBossTeleportUsable());
    // 発電機も使用可能なポータルもないステージでは、存在しない目的を表示しない
    if (generators_.empty() && !showPortal) {
        return;
    }

    const std::string countText = std::to_string(activeCount) + "/" + std::to_string(generators_.size());
    objectivePanel_->SetSize({ showPortal ? 320.0f :
        std::max(416.0f, 348.0f + static_cast<float>(countText.size()) * 16.0f), 76.0f });
    objectivePanel_->Update();
    objectivePanel_->Draw();
    objectiveLabel_->SetTextureLeftTop({ 0.0f, showPortal ? 128.0f : 0.0f });
    objectiveLabel_->Update();
    objectiveLabel_->Draw();
    if (showPortal) {
        return;
    }

    // 台数を固定せず、レベル再読み込みや桁数の変更にも対応する
    if (objectiveCountSprites_.size() != countText.size()) {
        objectiveCountSprites_.clear();
        for (size_t index = 0; index < countText.size(); ++index) {
            auto digit = std::make_unique<Sprite>();
            digit->Initialize(context_.spriteCommon, directionalLightResource_.Get(), "Resources/hud/count_glyphs.png");
            digit->SetTextureSize({ 32.0f, 64.0f });
            digit->SetSize({ 16.0f, 32.0f });
            digit->SetColor({ 1.0f, 0.9f, 0.3f, 1.0f });
            objectiveCountSprites_.push_back(std::move(digit));
        }
    }
    for (size_t index = 0; index < countText.size(); ++index) {
        // 画像は数字0〜9とスラッシュの順に横へ並べてある
        const int glyph = countText[index] == '/' ? 10 : countText[index] - '0';
        auto& digit = objectiveCountSprites_[index];
        digit->SetTextureLeftTop({ static_cast<float>(glyph) * 32.0f, 0.0f });
        digit->SetPosition({ 352.0f + static_cast<float>(index) * 16.0f, 122.0f });
        digit->Update();
        digit->Draw();
    }
}

void GamePlayScene::InitializeGeneratorUi()
{
    // 白い画像に色を付けて、パネルとメーターの各部品を作る
    auto createSprite = [this](const char* texture) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(context_.spriteCommon, directionalLightResource_.Get(), texture);
        sprite->SetAnchorPoint({ 0.0f, 0.0f });
        return sprite;
    };
    generatorUiPanel_ = createSprite("Resources/white2x2.png");
    generatorUiPanel_->SetColor({ 0.02f, 0.025f, 0.03f, 0.88f });
    generatorGaugeFrame_ = createSprite("Resources/white2x2.png");
    generatorGaugeFrame_->SetColor({ 0.75f, 0.75f, 0.65f, 1.0f });
    generatorGaugeBackground_ = createSprite("Resources/white2x2.png");
    generatorGaugeBackground_->SetColor({ 0.08f, 0.09f, 0.1f, 1.0f });
    generatorGaugeFill_ = createSprite("Resources/white2x2.png");

    // 日本語ラベルは画像から切り出し、ImGuiを使わない構成でも表示する
    generatorUiLabel_ = createSprite("Resources/generator/interaction_labels.png");
    generatorUiLabel_->SetTextureSize({ 512.0f, 64.0f });
}

void GamePlayScene::UpdateGeneratorInteraction(double deltaSeconds)
{
    // 死亡中は入力と進捗を停止する。デバッグカメラ中は呼び出し元で更新を止める
    if (!player_ || player_->IsDead()) {
        return;
    }

    // Eを一度押した後は、離れても長押しせずに起動が進む
    if (interactingGeneratorIndex_ >= 0) {
        auto& generator = generators_[interactingGeneratorIndex_];
        if (generator.activation.Update(deltaSeconds)) {
            generatorCompleteNoticeSeconds_ = kGeneratorCompleteNoticeSeconds;
        } else if (generator.activation.GetState() == GeneratorActivation::State::Active) {
            generatorCompleteNoticeSeconds_ = std::max(0.0, generatorCompleteNoticeSeconds_ - deltaSeconds);
        }

        // 起動中のメーターを優先し、同時に別の発電機を操作しない
        if (generator.activation.GetState() == GeneratorActivation::State::Starting ||
            generatorCompleteNoticeSeconds_ > 0.0) {
            generatorUiIndex_ = interactingGeneratorIndex_;
            return;
        }
        interactingGeneratorIndex_ = -1;
    }

    // 発電機の大きさが変わっても、表面からの距離で近づいたかを判定する
    const SphereCollider playerCollider = player_->GetCollider();
    const float interactionRadius = playerCollider.radius + kGeneratorInteractionRange;
    float nearestDistanceSq = interactionRadius * interactionRadius;
    int nearestActiveIndex = -1;
    float nearestActiveDistanceSq = nearestDistanceSq;
    for (size_t index = 0; index < generators_.size(); ++index) {
        const auto& generator = generators_[index];
        const Vector3& center = generator.collider.center;
        const Vector3& size = generator.collider.size;
        const float dx = playerCollider.center.x - std::clamp(
            playerCollider.center.x, center.x - size.x * 0.5f, center.x + size.x * 0.5f);
        const float dy = playerCollider.center.y - std::clamp(
            playerCollider.center.y, center.y - size.y * 0.5f, center.y + size.y * 0.5f);
        const float dz = playerCollider.center.z - std::clamp(
            playerCollider.center.z, center.z - size.z * 0.5f, center.z + size.z * 0.5f);
        const float distanceSq = dx * dx + dy * dy + dz * dz;

        // 起動済みの発電機より、操作できる未起動の発電機を優先する
        if (generator.activation.GetState() == GeneratorActivation::State::Idle) {
            if (distanceSq <= nearestDistanceSq) {
                nearestDistanceSq = distanceSq;
                generatorUiIndex_ = static_cast<int>(index);
            }
        } else if (distanceSq <= nearestActiveDistanceSq) {
            nearestActiveDistanceSq = distanceSq;
            nearestActiveIndex = static_cast<int>(index);
        }
    }

    if (generatorUiIndex_ < 0) {
        // 起動済みの発電機に近づいたときは完了表示だけを出す
        generatorUiIndex_ = nearestActiveIndex;
        return;
    }
    if (context_.input && context_.input->TriggerKey(DIK_E) &&
        generators_[generatorUiIndex_].activation.TryStart()) {
        interactingGeneratorIndex_ = generatorUiIndex_;
        generatorCompleteNoticeSeconds_ = 0.0;
    }
}

void GamePlayScene::UpdateGeneratorWorld(double deltaSeconds)
{
    size_t activeCount = 0;
    for (auto& generator : generators_) {
        const auto state = generator.activation.GetState();
        if (state == GeneratorActivation::State::Active) {
            ++activeCount;
        }
        if (state == GeneratorActivation::State::Idle) {
            continue;
        }
        // 起動開始から稼働音をループし、プレイヤーとの距離で音量を落とす
        if (!generator.motorSound && context_.sound) {
            generator.motorSound = context_.sound->CreateLoopingSound(generatorMotorSound_);
        }
        if (generator.motorSound && player_) {
            const Vector3 listener = player_->GetWorldPosition();
            const float dx = listener.x - generator.collider.center.x;
            const float dz = listener.z - generator.collider.center.z;
            const float attenuation = std::clamp(1.0f - std::hypot(dx, dz) / kGeneratorSoundRadius, 0.0f, 1.0f);
            generator.motorSound->SetVolume(kGeneratorMotorVolume * attenuation);
        }
    }

    // 全台のメーターが満タンになった時だけ、ステージ内のドアを開く
    generatorDoorLink_.Update(generators_.size(), activeCount, deltaSeconds);
    const float openProgress = generatorDoorLink_.GetOpenProgress();
    for (const auto& door : doors_) {
        Vector3 position = door.closedPosition;
        position.y += door.liftDistance * openProgress;
        door.object->SetTranslate(position);
        door.object->Update();
        // 配列要素は消さず、開き切ったら当たり判定を無効にする
        auto& collider = wallColliders_[door.colliderIndex];
        collider.center = door.closedColliderCenter;
        collider.center.y += door.liftDistance * openProgress;
        collider.hasCollider = !generatorDoorLink_.IsOpen();
    }
    if (minimap_) {
        minimap_->SetDoorsOpen(generatorDoorLink_.IsOpen());
    }

    // 聞こえる中で最も近い発電機を知らせ、音声出力の有無によらず敵が反応する
    std::vector<Vector3> reservedGeneratorPositions;
    for (const auto& enemy : enemies_) {
        Vector3 destination;
        if (enemy->GetGeneratorDestination(destination)) {
            reservedGeneratorPositions.push_back(destination);
        }
    }
    for (const auto& enemy : enemies_) {
        Vector3 destination;
        if (enemy->IsDead() || enemy->GetGeneratorDestination(destination)) {
            continue;
        }
        const Vector3 position = enemy->GetWorldPosition();
        const GeneratorData* nearestGenerator = nullptr;
        float nearestDistanceSq = kGeneratorSoundRadius * kGeneratorSoundRadius;
        for (const auto& generator : generators_) {
            if (generator.activation.GetState() == GeneratorActivation::State::Idle) {
                continue;
            }
            const float dx = position.x - generator.collider.center.x;
            const float dz = position.z - generator.collider.center.z;
            const float distanceSq = dx * dx + dz * dz;
            if (distanceSq <= nearestDistanceSq) {
                nearestDistanceSq = distanceSq;
                nearestGenerator = &generator;
            }
        }
        if (nearestGenerator) {
            enemy->OnHearGenerator(nearestGenerator->collider.center, nearestGenerator->collider.size,
                reservedGeneratorPositions);
            if (enemy->GetGeneratorDestination(destination)) {
                reservedGeneratorPositions.push_back(destination);
            }
        }
    }
}

bool GamePlayScene::IsBossTeleportUsable() const
{
    // ドアも発電機も無いステージでは最初から踏める。ある場合は全ドアが開き切ってから
    if (doors_.empty() && generators_.empty()) {
        return true;
    }
    return generatorDoorLink_.IsOpen();
}

void GamePlayScene::UpdateTeleporterEffects()
{
    const bool isUsable = !bossTeleports_.empty() && IsBossTeleportUsable();
    if (minimap_) {
        minimap_->SetTeleportersActive(isUsable);
    }
    if (!isUsable || !particleSystem_) {
        teleporterEffectTime_ = 0.0f;
        return;
    }

    constexpr float kDeltaTime = 1.0f / 60.0f;
    constexpr float kTwoPi = 6.28318530f;
    // 螺旋の本数・回転速度・上昇速度
    constexpr int kSpiralArmCount = 3;
    constexpr float kSpinSpeed = 4.0f;
    constexpr float kRiseSpeed = 1.6f;
    constexpr float kLifeTime = 1.1f;
    teleporterEffectTime_ += kDeltaTime;

    for (const BossTeleportData& teleport : bossTeleports_) {
        // テレポーターの外周を回る半径と、足元の高さ
        const float radius = std::max(teleport.size.x, teleport.size.z) * 0.5f + 0.3f;
        const float baseY = teleport.center.y - teleport.size.y * 0.5f + 0.1f;

        for (int arm = 0; arm < kSpiralArmCount; ++arm) {
            // 発生位置を毎フレーム回転させ、上へ昇るくるくるした螺旋を作る
            const float angle = teleporterEffectTime_ * kSpinSpeed +
                kTwoPi * static_cast<float>(arm) / static_cast<float>(kSpiralArmCount);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const Vector3 position{ teleport.center.x + c * radius, baseY, teleport.center.z + s * radius };
            // 回転方向(接線)へ少し流しながら上昇させる
            const Vector3 velocity{ -s * radius * 1.2f, kRiseSpeed, c * radius * 1.2f };

            particleSystem_->Emit(position, { 0.45f, 0.45f, 0.45f }, velocity,
                { 0.2f, 0.6f, 1.0f, 0.85f }, kLifeTime);
            // 中心に白い光を重ねて青い粒を目立たせる
            particleSystem_->Emit(position, { 0.2f, 0.2f, 0.2f }, velocity,
                { 0.85f, 0.95f, 1.0f, 1.0f }, kLifeTime * 0.8f);
        }
    }
}

void GamePlayScene::DrawGeneratorUi()
{
    if (generatorUiIndex_ < 0 || !player_ || player_->IsDead() ||
        (context_.isDebugMode && *context_.isDebugMode)) {
        return;
    }

    const auto& activation = generators_[generatorUiIndex_].activation;
    const bool isIdle = activation.GetState() == GeneratorActivation::State::Idle;
    const bool isActive = activation.GetState() == GeneratorActivation::State::Active;
    const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
    const float topY = static_cast<float>(WinApp::kClientHeight) - 150.0f;

    // 画面下中央に操作案内を表示し、起動を始めたらその下へメーターを出す
    context_.spriteCommon->CommonDrawSetting();
    generatorUiPanel_->SetPosition({ centerX - 200.0f, topY });
    generatorUiPanel_->SetSize({ 400.0f, isIdle ? 64.0f : 100.0f });
    generatorUiPanel_->Update();
    generatorUiPanel_->Draw();

    // ラベル画像の上から、操作案内・起動中・起動完了を切り替える
    const float labelRow = isIdle ? 0.0f : (isActive ? 2.0f : 1.0f);
    generatorUiLabel_->SetTextureLeftTop({ 0.0f, labelRow * 64.0f });
    generatorUiLabel_->SetPosition({ centerX - 180.0f, topY + 10.0f });
    generatorUiLabel_->SetSize({ 360.0f, 45.0f });
    generatorUiLabel_->SetColor(isActive
        ? Vector4{ 0.3f, 1.0f, 0.45f, 1.0f } : Vector4{ 1.0f, 0.9f, 0.2f, 1.0f });
    generatorUiLabel_->Update();
    generatorUiLabel_->Draw();

    if (!isIdle) {
        // メーターは左から右へ伸び、起動完了時は満タンの緑色にする
        generatorGaugeFrame_->SetPosition({ centerX - 162.0f, topY + 66.0f });
        generatorGaugeFrame_->SetSize({ 324.0f, 18.0f });
        generatorGaugeFrame_->Update();
        generatorGaugeFrame_->Draw();
        generatorGaugeBackground_->SetPosition({ centerX - 160.0f, topY + 68.0f });
        generatorGaugeBackground_->SetSize({ 320.0f, 14.0f });
        generatorGaugeBackground_->Update();
        generatorGaugeBackground_->Draw();
        if (activation.GetProgress() > 0.0f) {
            generatorGaugeFill_->SetPosition({ centerX - 160.0f, topY + 68.0f });
            generatorGaugeFill_->SetSize({ 320.0f * activation.GetProgress(), 14.0f });
            generatorGaugeFill_->SetColor(isActive
                ? Vector4{ 0.2f, 0.9f, 0.35f, 1.0f } : Vector4{ 1.0f, 0.85f, 0.1f, 1.0f });
            generatorGaugeFill_->Update();
            generatorGaugeFill_->Draw();
        }
    }
}

void GamePlayScene::UpdateAmmoUiSprites()
{
    if (!player_) {
        return;
    }

    // 近距離攻撃中でも、弾UIは選択中の銃を表示する
    const Player::AttackMode attackMode = player_->GetSelectedGunMode();
    const int maxAmmo = player_->GetCurrentMaxAmmo();
    const int currentAmmo = player_->GetCurrentAmmo();
    const bool isAssaultRifle = attackMode == Player::AttackMode::AssaultRifle;
    const bool isShotgun = attackMode == Player::AttackMode::Shotgun;

    // 武器ごとに弾UIの大きさと並びを変える
    AmmoUiLayout layout{};
    layout.bulletSize = isAssaultRifle
        ? Vector2{ 10.0f, 24.0f }
        : (isShotgun ? Vector2{ 22.0f, 38.0f } : Vector2{ 18.0f, 34.0f });
    layout.gapX = isAssaultRifle ? 5.0f : (isShotgun ? 10.0f : 8.0f);
    layout.gapY = isAssaultRifle ? 7.0f : 0.0f;
    layout.bulletsPerRow = isAssaultRifle ? 15 : (isShotgun ? 6 : 10);
    layout.startX = 32.0f;
    layout.startY = static_cast<float>(WinApp::kClientHeight) - 58.0f;

    // ショットガンだけ残弾UIの弾を赤くする
    const Vector4 ammoColor = isShotgun
        ? Vector4{ 1.0f, 0.18f, 0.08f, 0.95f }
        : Vector4{ 1.0f, 0.86f, 0.10f, 0.95f };

    // 武器が替わったら並びが変わるので、アニメーションせず即座に整列させる
    const bool layoutChanged =
        (attackMode != ammoUiPrevMode_) || (maxAmmo != ammoUiPrevMaxAmmo_);
    if (layoutChanged) {
        for (int index = 0;
             index < currentAmmo && index < static_cast<int>(ammoBulletAnims_.size());
             ++index) {
            ammoBulletAnims_[index].position =
                GetAmmoBulletPosition(layout, currentAmmo, index);
            ammoBulletAnims_[index].alpha = 0.95f;
            ammoBulletAnims_[index].delay = 0.0f;
        }
        for (AmmoFireEffect& effect : ammoFireEffects_) {
            effect.active = false;
        }
    } else if (currentAmmo < ammoUiPrevAmmo_) {
        // 撃った弾は出口(下段の右端)の位置から前へ飛び出して消える
        const Vector2 exitPosition =
            GetAmmoBulletPosition(layout, ammoUiPrevAmmo_, ammoUiPrevAmmo_ - 1);
        const Vector2 exitCenter = {
            exitPosition.x + layout.bulletSize.x * 0.5f,
            exitPosition.y
        };
        for (int shot = 0; shot < ammoUiPrevAmmo_ - currentAmmo; ++shot) {
            SpawnAmmoFireEffect(exitCenter, layout.bulletSize);
        }
    } else if (currentAmmo > ammoUiPrevAmmo_) {
        // リロードで増えた弾は左端の外から順番に入ってくる
        // 残っていた弾は出口側のスロットに居座り、新しい弾を奥へ詰める
        const int addedCount = currentAmmo - ammoUiPrevAmmo_;
        for (int index = ammoUiPrevAmmo_ - 1; index >= 0; --index) {
            ammoBulletAnims_[index + addedCount] = ammoBulletAnims_[index];
        }
        const float entryX = layout.startX - layout.bulletSize.x - 40.0f;
        for (int index = 0; index < addedCount; ++index) {
            const Vector2 target =
                GetAmmoBulletPosition(layout, currentAmmo, index);
            ammoBulletAnims_[index].position = { entryX, target.y };
            ammoBulletAnims_[index].alpha = 0.0f;
            // 奥(右寄り)のスロットへ入る弾から順番に装填する
            ammoBulletAnims_[index].delay =
                static_cast<float>(addedCount - 1 - index) * 2.0f;
        }
    }

    ammoUiPrevAmmo_ = currentAmmo;
    ammoUiPrevMaxAmmo_ = maxAmmo;
    ammoUiPrevMode_ = attackMode;

    // UI全体の後ろへ黒い半透明パネルを敷いて見えやすくする
    if (ammoBackgroundSprite_) {
        const float padding = 10.0f;
        const int rowCount =
            (maxAmmo + layout.bulletsPerRow - 1) / layout.bulletsPerRow;
        const float width =
            static_cast<float>(layout.bulletsPerRow) *
                (layout.bulletSize.x + layout.gapX) -
            layout.gapX + padding * 2.0f;
        const float height =
            static_cast<float>(rowCount) * (layout.bulletSize.y + layout.gapY) -
            layout.gapY + padding * 2.0f;
        const float topY = layout.startY -
            static_cast<float>(rowCount - 1) * (layout.bulletSize.y + layout.gapY) -
            layout.bulletSize.y * 0.5f - padding;
        ammoBackgroundSprite_->SetPosition({ layout.startX - padding, topY });
        ammoBackgroundSprite_->SetSize({ width, height });
        ammoBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.35f });
        ammoBackgroundSprite_->Update();
    }

    // 残っている弾を目標スロットへ滑らかに寄せる
    for (size_t index = 0; index < ammoSprites_.size(); ++index) {
        Sprite* sprite = ammoSprites_[index].get();
        if (!sprite) {
            continue;
        }

        // 使わないSpriteは透明にして描画に出ないようにする
        if (static_cast<int>(index) >= currentAmmo) {
            sprite->SetColor({ ammoColor.x, ammoColor.y, ammoColor.z, 0.0f });
            sprite->Update();
            continue;
        }

        AmmoBulletAnim& anim = ammoBulletAnims_[index];
        const Vector2 target =
            GetAmmoBulletPosition(layout, currentAmmo, static_cast<int>(index));

        // リロード直後の弾は時間差を消化してから入ってくる
        if (anim.delay > 0.0f) {
            anim.delay -= 1.0f;
        } else {
            anim.position.x += (target.x - anim.position.x) * 0.30f;
            anim.position.y += (target.y - anim.position.y) * 0.30f;
            anim.alpha += (0.95f - anim.alpha) * 0.35f;
        }

        sprite->SetPosition(anim.position);
        sprite->SetSize(layout.bulletSize);
        sprite->SetColor({ ammoColor.x, ammoColor.y, ammoColor.z, anim.alpha });
        sprite->Update();
    }

    UpdateAmmoFireEffects();
}

void GamePlayScene::SpawnAmmoFireEffect(const Vector2& center, const Vector2& size)
{
    // 空いている演出スロットを1つだけ使う
    for (AmmoFireEffect& effect : ammoFireEffects_) {
        if (effect.active) {
            continue;
        }
        effect.active = true;
        effect.position = center;
        effect.size = size;
        effect.timer = 0.0f;
        return;
    }
}

void GamePlayScene::UpdateAmmoFireEffects()
{
    // 発射演出の全体フレーム数
    constexpr float kDuration = 10.0f;

    for (size_t index = 0;
         index < ammoFireEffects_.size() && index < ammoFireEffectSprites_.size();
         ++index) {
        AmmoFireEffect& effect = ammoFireEffects_[index];
        Sprite* sprite = ammoFireEffectSprites_[index].get();
        if (!sprite) {
            continue;
        }

        if (effect.active) {
            effect.timer += 1.0f;
            if (effect.timer >= kDuration) {
                effect.active = false;
            }
        }
        if (!effect.active) {
            sprite->SetColor({ 1.0f, 0.9f, 0.35f, 0.0f });
            sprite->Update();
            continue;
        }

        // 勢いよく飛び出してすぐ減速する動きにする
        const float t = effect.timer / kDuration;
        const float pop = 1.0f - (1.0f - t) * (1.0f - t);

        // サイズは変えずにそのまま真上へ勢いよく飛ばす
        const Vector2 position = {
            effect.position.x,
            effect.position.y - 70.0f * pop
        };

        // 飛びながら徐々に薄くなっていき、上がり切ったところで消える
        const float alpha = 0.95f * (1.0f - t);

        sprite->SetPosition(position);
        sprite->SetSize(effect.size);
        sprite->SetColor({ 1.0f, 0.9f, 0.35f, alpha });
        sprite->Update();
    }
}

void GamePlayScene::DrawAmmoUiSprites()
{
    if (!player_) {
        return;
    }

    const int currentAmmo = player_->GetCurrentAmmo();

    // 奥から順に、背景パネル → 残弾 → 発射演出の順で重ねる
    if (ammoBackgroundSprite_) {
        ammoBackgroundSprite_->Draw();
    }
    for (int index = 0;
         index < currentAmmo && index < static_cast<int>(ammoSprites_.size());
         ++index) {
        if (ammoSprites_[index]) {
            ammoSprites_[index]->Draw();
        }
    }
    for (size_t index = 0;
         index < ammoFireEffects_.size() && index < ammoFireEffectSprites_.size();
         ++index) {
        if (ammoFireEffects_[index].active && ammoFireEffectSprites_[index]) {
            ammoFireEffectSprites_[index]->Draw();
        }
    }
}

void GamePlayScene::UpdateBossHpUiSprites()
{
    constexpr float kBarWidth = 520.0f;
    constexpr float kBarHeight = 24.0f;
    constexpr float kFramePadding = 4.0f;
    const Vector2 barPosition = {
        static_cast<float>(WinApp::kClientWidth) * 0.5f,
        28.0f
    };

    Boss* activeBoss = nullptr;
    for (const auto& boss : bosses_) {
        if (!boss || boss->IsReadyToRemove()) {
            continue;
        }
        activeBoss = boss.get();
        break;
    }

    if (!activeBoss) {
        // ボスがいない時はHPバーを透明にする
        if (bossHpBackgroundSprite_) {
            bossHpBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            bossHpBackgroundSprite_->Update();
        }
        if (bossHpFillSprite_) {
            bossHpFillSprite_->SetColor({ 0.95f, 0.05f, 0.03f, 0.0f });
            bossHpFillSprite_->Update();
        }
        if (bossHpFrameSprite_) {
            bossHpFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
            bossHpFrameSprite_->Update();
        }
        return;
    }

    const float hpRate = std::clamp(
        static_cast<float>(activeBoss->GetHp()) /
            static_cast<float>(activeBoss->GetMaxHp()),
        0.0f,
        1.0f);

    if (bossHpFrameSprite_) {
        // 白い外枠を少し大きく敷く
        bossHpFrameSprite_->SetPosition({
            barPosition.x,
            barPosition.y - kFramePadding
        });
        bossHpFrameSprite_->SetSize({
            kBarWidth + kFramePadding * 2.0f,
            kBarHeight + kFramePadding * 2.0f
        });
        bossHpFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.85f });
        bossHpFrameSprite_->Update();
    }

    if (bossHpBackgroundSprite_) {
        // HPが減った後も最大HP幅が分かるように黒背景を置く
        bossHpBackgroundSprite_->SetPosition(barPosition);
        bossHpBackgroundSprite_->SetSize({ kBarWidth, kBarHeight });
        bossHpBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.75f });
        bossHpBackgroundSprite_->Update();
    }

    if (bossHpFillSprite_) {
        // 左端を固定して、HP割合分だけ赤バーを縮める
        bossHpFillSprite_->SetPosition({
            barPosition.x - kBarWidth * 0.5f,
            barPosition.y
        });
        bossHpFillSprite_->SetSize({ kBarWidth * hpRate, kBarHeight });
        bossHpFillSprite_->SetColor({ 0.95f, 0.05f, 0.03f, 0.95f });
        bossHpFillSprite_->Update();
    }
}

void GamePlayScene::DrawBossHpUiSprites()
{
    if (bosses_.empty()) {
        return;
    }

    // 奥から外枠、背景、残量の順で描画する
    if (bossHpFrameSprite_) {
        bossHpFrameSprite_->Draw();
    }
    if (bossHpBackgroundSprite_) {
        bossHpBackgroundSprite_->Draw();
    }
    if (bossHpFillSprite_) {
        bossHpFillSprite_->Draw();
    }
}
void GamePlayScene::EmitMeleeSlashEffect(const Vector3& center, const Vector3& direction)
{
    if (!particleSystem_) {
        return;
    }

    Vector3 slashDirection = { direction.x, 0.0f, direction.z };
    if (slashDirection.x == 0.0f && slashDirection.z == 0.0f) {
        slashDirection = { 0.0f, 0.0f, 1.0f };
    } else {
        slashDirection = Normalize(slashDirection);
    }

    // 球を細長く伸ばした斬撃を1個だけ出す
    const Vector3 slashPosition = {
        center.x,
        center.y + 1.0f,
        center.z
    };

    static std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_int_distribution<int> randomDirection(0, 1);
    const bool isVerticalSlash = randomDirection(randomEngine) == 0;
    const Vector3 slashScale = isVerticalSlash
        ? Vector3{ 0.28f, 1.95f, 0.28f }
        : Vector3{ 1.95f, 0.28f, 0.28f };

    // エンジン側を変えず、縦長か横長かをランダムに切り替える
    particleSystem_->Emit(
        slashPosition,
        slashScale,
        { 0.0f, 0.0f, 0.0f },
        { 0.95f, 0.98f, 1.0f, 0.90f },
        0.11f);
}

void GamePlayScene::UpdateMeleeAttack()
{
    if (!player_ || player_->IsDead() || !context_.input) {
        return;
    }

    // ナイフモードではないときは近接ターゲットを表示しない
    if (!player_->IsMeleeMode() && !isMeleeAttacking_) {
        meleeTarget_ = nullptr;
        return;
    }

    if (isMeleeAttacking_) {
        meleeAttackTimer_++;

        // 攻撃中に対象が倒れた場合は近接攻撃を中断する
        if (!meleeVictim_ || meleeVictim_->IsDead()) {
            isMeleeAttacking_ = false;
            meleeVictim_ = nullptr;
            meleeAttackTimer_ = 0;
            return;
        }

        const float approachRate =
            static_cast<float>(meleeAttackTimer_) /
            static_cast<float>(meleeAttackHitFrame_);
        player_->SetPosition(
            Lerp(meleeStartPosition_, meleeStrikePosition_, approachRate));

        if (meleeAttackTimer_ >= meleeAttackHitFrame_) {
            player_->SetPosition(meleeStrikePosition_);

            const Vector3 victimPosition = meleeVictim_->GetWorldPosition();
            Vector3 slashDirection = {
                victimPosition.x - meleeStartPosition_.x,
                0.0f,
                victimPosition.z - meleeStartPosition_.z
            };

            // ヒットした瞬間にナイフの斬撃エフェクトを出す
            EmitMeleeSlashEffect(victimPosition, slashDirection);
            meleeVictim_->OnMeleeHit();

            isMeleeAttacking_ = false;
            meleeVictim_ = nullptr;
            meleeAttackTimer_ = 0;
        }
        return;
    }

    meleeTarget_ = nullptr;

    const Vector3 playerPosition = player_->GetWorldPosition();
    float nearestInstantKillDistanceSq = meleeAttackRange_ * meleeAttackRange_;

    for (auto& enemy : enemies_) {
        // 死んでいる敵は近接攻撃の対象にしない
        if (enemy->IsDead()) {
            continue;
        }

        const Vector3 enemyPosition = enemy->GetWorldPosition();
        const float diffX = enemyPosition.x - playerPosition.x;
        const float diffZ = enemyPosition.z - playerPosition.z;
        const float distanceSq = diffX * diffX + diffZ * diffZ;

        if (!enemy->HasDetectedPlayer() && distanceSq <= nearestInstantKillDistanceSq) {
            // 瞬殺できる特別近接は、まだプレイヤーを発見していない敵だけ対象にする
            nearestInstantKillDistanceSq = distanceSq;
            meleeTarget_ = enemy.get();
        }
    }

    if (!context_.input->TriggerMouseLeft()) {
        return;
    }

    if (meleeTarget_) {
        // 瞬殺できる敵がいるときだけ、踏み込み付きの特別近接を開始する
        meleeVictim_ = meleeTarget_;
        meleeStartPosition_ = playerPosition;
        meleeAttackTimer_ = 0;
        isMeleeAttacking_ = true;

        const Vector3 victimPosition = meleeVictim_->GetWorldPosition();
        Vector3 toVictim = {
            victimPosition.x - playerPosition.x,
            0.0f,
            victimPosition.z - playerPosition.z
        };
        if (toVictim.x == 0.0f && toVictim.z == 0.0f) {
            toVictim = { 0.0f, 0.0f, 1.0f };
        } else {
            toVictim = Normalize(toVictim);
        }

        // 敵と重ならないように、敵の少し手前まで踏み込む
        meleeStrikePosition_ = {
            victimPosition.x - toVictim.x * 1.2f,
            playerPosition.y,
            victimPosition.z - toVictim.z * 1.2f
        };

        // 攻撃開始時にも薄い斬撃を出して、入力したことを分かりやすくする
        EmitMeleeSlashEffect(playerPosition, toVictim);
        meleeTarget_ = nullptr;
        return;
    }

    Vector3 slashDirection = PlayerBullet::CalcDirectionToMouseGround(
        playerPosition,
        context_.camera,
        context_.input);

    if (slashDirection.x == 0.0f && slashDirection.z == 0.0f) {
        slashDirection = { 0.0f, 0.0f, 1.0f };
    } else {
        slashDirection = Normalize(slashDirection);
    }

    // 普通の近接は判定範囲の中心へ斬撃エフェクトを出す
    const float normalMeleeEffectDistance = normalMeleeAttackRange_ * 0.5f;
    const Vector3 normalMeleeEffectPosition = {
        playerPosition.x + slashDirection.x * normalMeleeEffectDistance,
        playerPosition.y,
        playerPosition.z + slashDirection.z * normalMeleeEffectDistance
    };

    // 吸い付きなしで、見た目と同じ位置に斬撃を出す
    EmitMeleeSlashEffect(normalMeleeEffectPosition, slashDirection);

    Enemy* normalMeleeHitTarget = nullptr;
    float nearestNormalHitForward = normalMeleeAttackRange_;
    constexpr float kNormalMeleeHalfWidth = 1.25f;

    for (auto& enemy : enemies_) {
        // 死んでいる敵には通常近接を当てない
        if (enemy->IsDead()) {
            continue;
        }

        const Vector3 enemyPosition = enemy->GetWorldPosition();
        Vector3 toEnemy = {
            enemyPosition.x - playerPosition.x,
            0.0f,
            enemyPosition.z - playerPosition.z
        };

        const float forwardDistance =
            toEnemy.x * slashDirection.x +
            toEnemy.z * slashDirection.z;
        if (forwardDistance < 0.0f || forwardDistance > normalMeleeAttackRange_) {
            continue;
        }

        const float sideX = toEnemy.x - slashDirection.x * forwardDistance;
        const float sideZ = toEnemy.z - slashDirection.z * forwardDistance;
        const float sideDistanceSq = sideX * sideX + sideZ * sideZ;
        if (sideDistanceSq > kNormalMeleeHalfWidth * kNormalMeleeHalfWidth) {
            continue;
        }

        // 斬撃の帯に入った一番手前の敵だけに1ダメージを与える
        if (forwardDistance <= nearestNormalHitForward) {
            nearestNormalHitForward = forwardDistance;
            normalMeleeHitTarget = enemy.get();
        }
    }

    if (normalMeleeHitTarget) {
        normalMeleeHitTarget->OnMeleeDamage(
            normalMeleeHitTarget->GetWorldPosition(),
            slashDirection);
    }
}

void GamePlayScene::Finalize()
{
    // 案内用Spriteも、参照しているライト用リソースより先に解放する
    objectivePanel_.reset();
    objectiveLabel_.reset();
    objectiveCountSprites_.clear();
    controlsGuide_.reset();
    // 発電機のUIをライト用リソースより先に解放する
    generatorUiPanel_.reset();
    generatorUiLabel_.reset();
    generatorGaugeFrame_.reset();
    generatorGaugeBackground_.reset();
    generatorGaugeFill_.reset();
    generators_.clear();
    doors_.clear();
    interactingGeneratorIndex_ = -1;
    generatorUiIndex_ = -1;
    sprites_.clear();
    ammoSprites_.clear();
    bossHpBackgroundSprite_.reset();
    bossHpFillSprite_.reset();
    bossHpFrameSprite_.reset();
    meleeMarker_.reset();
    meleeTarget_ = nullptr;
    meleeVictim_ = nullptr;
    isMeleeAttacking_ = false;
    meleeAttackTimer_ = 0;

    object3d_.reset();
    particleSystem_.reset();
    bloodParticleSystem_.reset();
    grenadeSmokeParticleSystem_.reset();
    skybox_.reset();
    skyboxCommon_.reset();
    line3dCommon_.reset();

    materialResource_.Reset();
    directionalLightResource_.Reset();

    initialized_ = false;

    debugCamera_.reset();
    player_.reset();
    enemies_.clear();
    bosses_.clear();

    // ミニマップが所有するSpriteを解放する
    minimap_.reset();
}

void GamePlayScene::StartBulletShockwaves()
{
    if (!player_ || !context_.camera || !context_.offscreenRenderer) {
        return;
    }

    // Playerが撃った弾の発射位置を受け取り、Scene側で画面UVへ変換する
    const std::vector<Vector3> shotPositions =
        player_->ConsumeBulletShockwavePositions();

    for (const Vector3& shotPosition : shotPositions) {
        Vector2 shockwaveUV{};
        if (!TryConvertWorldToScreenUV(
            shotPosition,
            context_.camera->GetViewProjectionMatrix(),
            shockwaveUV)) {
            continue;
        }

        // 画面上の弾の発射位置を中心にポストエフェクトの歪みを始める
        // 銃の衝撃波は短い時間で広げて、前より速く見せる
        context_.offscreenRenderer->SetShockwaveDuration(0.16f);
        context_.offscreenRenderer->StartShockwave(shockwaveUV);
    }
}

void GamePlayScene::CheckGrenadeExplosions()
{
    if (!player_) {
        return;
    }

    // このフレームに発生した全爆発位置をPlayerから受け取る
    const std::vector<Vector3> explosionPositions =
        player_->ConsumeGrenadeExplosions();
    const float explosionRadiusSq =
        grenadeExplosionRadius_ * grenadeExplosionRadius_;

    for (const Vector3& explosionPosition : explosionPositions) {
        // グレネードの爆発位置を画面UVに変換して、大きめの衝撃波を出す
        if (context_.camera && context_.offscreenRenderer) {
            Vector2 grenadeShockwaveUV{};
            if (TryConvertWorldToScreenUV(
                explosionPosition,
                context_.camera->GetViewProjectionMatrix(),
                grenadeShockwaveUV)) {
                // グレネードの衝撃波は通常時間に戻して、大きさだけ個別に変える
                context_.offscreenRenderer->SetShockwaveDuration(0.28f);
                context_.offscreenRenderer->StartShockwave(grenadeShockwaveUV, 0.22f);
            }
        }

        for (auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            const Vector3 enemyPosition = enemy->GetWorldPosition();
            const float diffX = enemyPosition.x - explosionPosition.x;
            const float diffY = enemyPosition.y - explosionPosition.y;
            const float diffZ = enemyPosition.z - explosionPosition.z;
            const float distanceSq =
                diffX * diffX + diffY * diffY + diffZ * diffZ;

            // 爆発半径内なら残りHPに関係なく即死させる
            if (distanceSq <= explosionRadiusSq) {
                enemy->OnExplosionHit();
            }
        }
    }
}
void GamePlayScene::CheckCollisions()
{
    if (!player_) {
        return;
    }

    // テレポートでレベルを作り直したフレームは、古い敵や弾との判定を続けない
    if (CheckBossTeleport()) {
        return;
    }

    for (auto& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        if (!isMeleeAttacking_ && Collision::IsHit(player_->GetCollider(), enemy->GetCollider())) {
            player_->OnHit();
        }
    }

    const auto& bullets = player_->GetBullets();
    for (const auto& bullet : bullets) {
        if (bullet->IsDead()) {
            continue;
        }

        for (auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            if (Collision::IsHit(bullet->GetCollider(), enemy->GetCollider())) {
                Vector3 hitDirection = Normalize(bullet->GetVelocity());
                SphereCollider enemyCollider = enemy->GetCollider();
                SphereCollider bulletCollider = bullet->GetCollider();

                // 弾が入ってきた側の敵コライダー表面を命中位置にする
                Vector3 hitPosition = {
                    enemyCollider.center.x - hitDirection.x * enemyCollider.radius,
                    bulletCollider.center.y,
                    enemyCollider.center.z - hitDirection.z * enemyCollider.radius
                };

                bullet->OnHit();
                enemy->OnHit(hitPosition, hitDirection);
                break;
            }
        }

        if (bullet->IsDead()) {
            continue;
        }

        for (auto& boss : bosses_) {
            if (boss->IsDead()) {
                continue;
            }

            if (Collision::IsHit(bullet->GetCollider(), boss->GetCollider())) {
                // ボスに弾が当たったら弾を消して、ボスHPを1減らす
                bullet->OnHit();
                boss->OnHit();
                break;
            }
        }
    }

    // 弾によるボス撃破を先に処理し、生きているボスの着地攻撃だけを当てる
    for (const auto& boss : bosses_) {
        if (!isMeleeAttacking_ && !player_->IsDead() && boss->IsAttackHit(player_->GetCollider())) {
            // 既存の無敵時間を使い、範囲内でも一撃につきHPを1だけ減らす
            player_->OnHit();
        }
    }

    enemies_.erase(
        std::remove_if(
            enemies_.begin(),
            enemies_.end(),
            [](const std::unique_ptr<Enemy>& enemy) {
                // 死亡直後には消さず、破片演出が終わってから削除する
                return enemy->IsReadyToRemove();
            }),
        enemies_.end());
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    if (!context_.camera) {
        return;
    }

    Transform& cameraTransform = context_.camera->GetTransform();

    ImGui::Begin("Camera");
    ImGui::DragFloat3("Translate", &cameraTransform.translate.x, 0.1f);
    ImGui::DragFloat3("Rotate", &cameraTransform.rotate.x, 0.01f);
    ImGui::Text("Skybox and objects use this camera.");
    ImGui::End();

    ImGui::Begin("Level Reload");
    ImGui::Text("F5 : Manual Reload");
    ImGui::Text("Watching : %s", levelHotReload_.GetFilePath().c_str());

    if (reloadNoticeFrameCount_ > 0) {
        ImGui::Text("%s", reloadNoticeText_.c_str());
    } else {
        ImGui::Text("Idle");
    }

    ImGui::End();

    if (context_.offscreenRenderer) {
        // 弾の発射時に出る衝撃波の見た目をゲームシーン側で調整する
        float shockwaveDuration = context_.offscreenRenderer->GetShockwaveDuration();
        float shockwaveSize = context_.offscreenRenderer->GetShockwaveMaxRadius();
        bool isWhiteWaveEnabled = context_.offscreenRenderer->IsShockwaveWhiteWaveEnabled();

        ImGui::Begin("Bullet Shockwave");
        if (ImGui::DragFloat("Duration", &shockwaveDuration, 0.01f, 0.05f, 1.0f)) {
            context_.offscreenRenderer->SetShockwaveDuration(shockwaveDuration);
        }
        if (ImGui::DragFloat("Size", &shockwaveSize, 0.01f, 0.05f, 0.40f)) {
            context_.offscreenRenderer->SetShockwaveMaxRadius(shockwaveSize);
        }
        if (ImGui::Checkbox("White Wave", &isWhiteWaveEnabled)) {
            context_.offscreenRenderer->SetShockwaveWhiteWaveEnabled(isWhiteWaveEnabled);
        }
        ImGui::End();
    }

    if (player_) {
        const char* equipName = "Unknown";
        if (player_->GetAttackMode() == Player::AttackMode::Gun) {
            equipName = "Gun";
        } else if (player_->GetAttackMode() == Player::AttackMode::AssaultRifle) {
            equipName = "Assault Rifle";
        } else if (player_->GetAttackMode() == Player::AttackMode::Shotgun) {
            equipName = "Shotgun";
        } else if (player_->GetAttackMode() == Player::AttackMode::Knife) {
            equipName = "Knife";
        }

        // ゲーム画面左上に現在装備をHUDとして表示する
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin(
            "Player Equipment",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Weapon : %s", equipName);
        ImGui::End();
    }

    if (object3d_) {
        // Object3d全体で共有しているライト設定をImGuiに表示する
        object3d_->DrawLightImGui();
    }

    if (context_.offscreenRenderer) {
        context_.offscreenRenderer->DrawDebugGameViewImGui();
        context_.offscreenRenderer->DrawImGui();
    }
#endif
}

void GamePlayScene::ResolveEnemyOverlap()
{
    for (size_t i = 0; i < enemies_.size(); ++i) {
        for (size_t j = i + 1; j < enemies_.size(); ++j) {
            Vector3 posA = enemies_[i]->GetWorldPosition();
            Vector3 posB = enemies_[j]->GetWorldPosition();

            Vector3 diff = {
                posB.x - posA.x,
                0.0f,
                posB.z - posA.z
            };

            float distanceSq = diff.x * diff.x + diff.z * diff.z;
            float radiusSum = enemies_[i]->GetBodyRadius() + enemies_[j]->GetBodyRadius();

            if (distanceSq <= 0.0001f) {
                diff = { 1.0f, 0.0f, 0.0f };
                distanceSq = 1.0f;
            }

            float distance = std::sqrt(distanceSq);

            if (distance < radiusSum) {
                float overlap = radiusSum - distance;

                Vector3 push = {
                    diff.x / distance,
                    0.0f,
                    diff.z / distance
                };

                posA.x -= push.x * overlap * 0.5f;
                posA.z -= push.z * overlap * 0.5f;

                posB.x += push.x * overlap * 0.5f;
                posB.z += push.z * overlap * 0.5f;

                enemies_[i]->SetPosition(posA);
                enemies_[j]->SetPosition(posB);
            }
        }
    }
}

void GamePlayScene::CreateMapObjects()
{
    floorObjects_.clear();
    wallObjects_.clear();
    floorColliders_.clear();
    wallColliders_.clear();
    bossTeleports_.clear();

    // ステージ再読み込み時は発電機を未起動状態へ戻し、古い操作対象を解除する
    generators_.clear();
    interactingGeneratorIndex_ = -1;
    generatorUiIndex_ = -1;
    generatorCompleteNoticeSeconds_ = 0.0;

    // 再読み込み後のドアは閉じた状態から始める
    doors_.clear();
    generatorDoorLink_ = GeneratorDoorLink{};

    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);
    navMeshData_ = levelData.navMesh;
    // 敵AI用のNavMeshデータを保存する
    // マップ用のモデルとコライダーを作る前に、レベル階層を平坦化する
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    // ミニマップ用JSONに依存せず、実際に配置する設置物からマーカーを作る
    std::vector<MinimapObjectData> minimapObjects;

    for (const LevelObjectData& objectData : allObjects) {
        if (objectData.type != "MESH") {
            continue;
        }

        if (objectData.name.rfind("Enemy_", 0) == 0 || objectData.name.rfind("enemy_", 0) == 0) {
            continue;
        }
        // PlayerモデルはPlayerクラスが管理するため、マップ側では作らない
        if (objectData.objectKind == "player" || objectData.name == "Player") {
            continue;
        }


        if (objectData.fileName.empty()) {
            continue;
        }

        ModelManager::GetInstance()->LoadModel(objectData.fileName);

        auto mapObject = std::make_unique<Object3d>();
        mapObject->Initialize(context_.object3dCommon);
        mapObject->SetModel(objectData.fileName);
        mapObject->SetScale(objectData.scaling);
        mapObject->SetRotate(objectData.rotation);
        mapObject->SetTranslate(objectData.translation);
        mapObject->Update();

        if (objectData.objectKind == "boss_teleport" || objectData.name.rfind("BossTeleport", 0) == 0) {
            BossTeleportData teleport{};

            if (objectData.collider.hasCollider && objectData.collider.type == "BOX") {
                teleport.center.x =
                    objectData.translation.x + objectData.collider.center.x * objectData.scaling.x;
                teleport.center.y =
                    objectData.translation.y + objectData.collider.center.y * objectData.scaling.y;
                teleport.center.z =
                    objectData.translation.z + objectData.collider.center.z * objectData.scaling.z;

                teleport.size.x = objectData.collider.size.x * objectData.scaling.x;
                teleport.size.y = objectData.collider.size.y * objectData.scaling.y;
                teleport.size.z = objectData.collider.size.z * objectData.scaling.z;
            } else {
                // コライダーが無い場合は、BlenderのCube基準で見た目と同じくらいの範囲を使う
                teleport.center = objectData.translation;
                teleport.size = {
                    objectData.scaling.x * 2.0f,
                    objectData.scaling.y * 2.0f,
                    objectData.scaling.z * 2.0f
                };
            }

            // JSONに移動先が無い場合でも、ボステレポーターは標準のボスステージへ飛ばす
            teleport.targetLevel = objectData.targetLevel.empty()
                ? "Resources/level/bossStage.json"
                : objectData.targetLevel;
            bossTeleports_.push_back(teleport);

            // ミニマップにも登録する。表示は踏めるようになってから青い丸で行う
            MinimapObjectData marker;
            marker.kind = MinimapObjectData::Kind::Teleporter;
            marker.position = teleport.center;
            marker.size = { teleport.size.x, teleport.size.z };
            minimapObjects.push_back(marker);

            // テレポーターは床や壁の移動コライダーに混ぜず、見た目だけ描画する
            floorObjects_.push_back(std::move(mapObject));
            continue;
        }

        if (objectData.collider.hasCollider && objectData.collider.type == "BOX") {
            LevelColliderData worldCollider = objectData.collider;

            worldCollider.center.x =
                objectData.translation.x + objectData.collider.center.x * objectData.scaling.x;
            worldCollider.center.y =
                objectData.translation.y + objectData.collider.center.y * objectData.scaling.y;
            worldCollider.center.z =
                objectData.translation.z + objectData.collider.center.z * objectData.scaling.z;

            worldCollider.size.x = objectData.collider.size.x * objectData.scaling.x;
            worldCollider.size.y = objectData.collider.size.y * objectData.scaling.y;
            worldCollider.size.z = objectData.collider.size.z * objectData.scaling.z;

            // 発電機とドアは、通り抜けできない設置物として壁側へ登録する
            const bool isPlacedObstacle =
                objectData.objectKind == "generator" || objectData.objectKind == "door";
            if (isPlacedObstacle) {
                // 回転・拡縮した設置物を囲むBOXを作り、Blenderでの配置に合わせる
                const Matrix4x4 world = MakeAffineMatrix(
                    objectData.scaling, objectData.rotation, objectData.translation);
                const Vector3& center = objectData.collider.center;
                const Vector3& size = objectData.collider.size;
                const float localCenter[] = { center.x, center.y, center.z };
                const float localSize[] = { size.x, size.y, size.z };
                float worldCenter[3]{};
                float worldSize[3]{};
                for (int axis = 0; axis < 3; ++axis) {
                    worldCenter[axis] = world.m[3][axis];
                    for (int localAxis = 0; localAxis < 3; ++localAxis) {
                        worldCenter[axis] += localCenter[localAxis] * world.m[localAxis][axis];
                        worldSize[axis] += std::fabs(localSize[localAxis] * world.m[localAxis][axis]);
                    }
                }
                worldCollider.center = { worldCenter[0], worldCenter[1], worldCenter[2] };
                worldCollider.size = { worldSize[0], worldSize[1], worldSize[2] };

                // 実際に配置した発電機のBOXと、個別の起動状態を登録する
                if (objectData.objectKind == "generator") {
                    GeneratorData generator;
                    generator.collider = worldCollider;
                    generators_.push_back(std::move(generator));
                }

                // モデルの横軸を上から見た方向へ投影し、ドアの向きも合わせる
                MinimapObjectData marker;
                marker.kind = objectData.objectKind == "generator"
                    ? MinimapObjectData::Kind::Generator : MinimapObjectData::Kind::Door;
                marker.position = worldCollider.center;
                marker.size = {
                    std::fabs(size.x) * std::hypot(world.m[0][0], world.m[0][2]),
                    std::fabs(size.z) * std::hypot(world.m[2][0], world.m[2][2])
                };
                // ミニマップではゲームのZ方向が画面の上方向になる
                marker.rotation = std::atan2(-world.m[0][2], world.m[0][0]);
                minimapObjects.push_back(marker);
            }

            if (isPlacedObstacle || objectData.name.find("Wall") != std::string::npos ||
                objectData.name.find("wall") != std::string::npos) {
                // ドアの閉じた位置と壁配列の番号を記憶し、後で両方を動かす
                if (objectData.objectKind == "door") {
                    DoorData door;
                    door.object = mapObject.get();
                    door.colliderIndex = wallColliders_.size();
                    door.closedPosition = objectData.translation;
                    door.closedColliderCenter = worldCollider.center;
                    door.liftDistance = worldCollider.size.y + 0.5f;
                    doors_.push_back(door);
                }
                wallColliders_.push_back(worldCollider);
                wallObjects_.push_back(std::move(mapObject));
            } else {
                floorColliders_.push_back(worldCollider);
                floorObjects_.push_back(std::move(mapObject));
            }
        } else {
            floorObjects_.push_back(std::move(mapObject));
        }
    }

    // 初回配置・F5再読み込み・ステージ移動のたびに設置物マーカーを更新する
    if (minimap_) {
        minimap_->SetObjectMarkers(minimapObjects);
    }
}

void GamePlayScene::SpawnEnemies()
{
    // 敵の再生成時は近接攻撃とキル演出の状態を解除する
    meleeTarget_ = nullptr;
    meleeVictim_ = nullptr;
    isMeleeAttacking_ = false;
    meleeAttackTimer_ = 0;

    // レベルデータから敵を作り直す
    enemies_.clear();

    // 現在のBlenderレベルJSONを読み込む
    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);

    // 敵オブジェクト名ごとに出現位置を保存する
    std::unordered_map<std::string, Vector3> enemySpawnMap;

    // 敵オブジェクト名と番号ごとにウェイポイント位置を保存する
    std::unordered_map<std::string, std::vector<std::pair<int, Vector3>>> enemyWaypointMap;

    // 敵オブジェクトを探す前にレベル階層を平坦化する
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        const std::string& name = objectData.name;

        // Enemy_00_Waypoint_00 のような名前は敵のウェイポイントとして扱う
        size_t waypointPos = name.find("_Waypoint_");
        if (waypointPos != std::string::npos) {
            std::string enemyName = name.substr(0, waypointPos);
            std::string indexText = name.substr(waypointPos + std::string("_Waypoint_").size());
            int waypointIndex = std::stoi(indexText);

            // 対応する敵の名前に、このウェイポイントを登録する
            enemyWaypointMap[enemyName].push_back({ waypointIndex, objectData.translation });
            continue;
        }

        // Enemyオブジェクトは敵の出現位置として扱う
        if (name.rfind("Enemy_", 0) == 0 || name.rfind("enemy_", 0) == 0) {
            if (objectData.fileName == "boss/boss.obj") {
                // ボスモデルはBossクラスで生成するため、通常敵には入れない
                continue;
            }

            // 後で敵を作るために出現位置を保存する
            enemySpawnMap[name] = objectData.translation;
        }
    }

    // 保存した出現位置から敵を生成する
    for (const auto& enemyEntry : enemySpawnMap) {
        const std::string& enemyName = enemyEntry.first;
        const Vector3& spawnPosition = enemyEntry.second;

        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(context_.object3dCommon, context_.camera, spawnPosition);
        enemy->SetBloodParticleSystem(bloodParticleSystem_.get());

        // 敵にもプレイヤーと同じマップとコライダー情報を渡す
        enemy->SetMap(&mapField_, tileSize_);
        enemy->SetFloorColliders(&floorColliders_);
        enemy->SetWallColliders(&wallColliders_);
        enemy->SetNavMesh(&navMeshData_);

        // 敵にNavMeshを渡して、壁を回り込めるようにする
        // 敵に渡す前にウェイポイントを番号順に並べる
        std::vector<Vector3> waypoints;
        auto found = enemyWaypointMap.find(enemyName);
        if (found != enemyWaypointMap.end()) {
            auto& waypointPairs = found->second;

            std::sort(
                waypointPairs.begin(),
                waypointPairs.end(),
                [](const std::pair<int, Vector3>& a, const std::pair<int, Vector3>& b) {
                    return a.first < b.first;
                });

            for (const auto& waypointPair : waypointPairs) {
                waypoints.push_back(waypointPair.second);
            }
        }

        // JSONから読み込んだウェイポイント一覧を敵に設定する
        enemy->SetWaypoints(waypoints);

        enemies_.push_back(std::move(enemy));
    }
}

void GamePlayScene::SpawnBosses()
{
    // レベルデータからボスを作り直す
    bosses_.clear();

    // 現在のBlenderレベルJSONを読み込む
    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);

    // ボスオブジェクトを探す前にレベル階層を平坦化する
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        if (objectData.fileName != "boss/boss.obj") {
            continue;
        }

        auto boss = std::make_unique<Boss>();
        boss->Initialize(
            context_.object3dCommon,
            context_.camera,
            objectData.translation,
            objectData.rotation,
            objectData.scaling);

        // 巨体がステージ外へ出ないように、壁の当たり判定を共有する
        boss->SetWallColliders(&wallColliders_);

        bosses_.push_back(std::move(boss));
    }
}
