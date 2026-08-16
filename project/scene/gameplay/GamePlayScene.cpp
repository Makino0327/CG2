#include "GamePlayScene.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <memory>
#include <cmath>
#include <random>
#include <unordered_map>

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

        // テレポーターを踏んだら、指定されたJSONを監視対象にしてレベルを読み直す
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
            if (objectData.objectKind == "player" || objectData.name == "Player") {
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
        // ゲームシーン側で通常の衝撃波サイズを調整する
        context_.offscreenRenderer->SetShockwaveMaxRadius(0.10f);
    }

    mapField_.LoadFromCsv("Resources/map.csv");
    player_->SetMap(&mapField_, tileSize_);

    // Initialize hot reload watching for the level JSON file.
    levelHotReload_.Initialize(levelFilePath_);
    // Load the level JSON and rebuild the player spawn, map objects, and enemies.
    ReloadLevel(false);
}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

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

    if (player_ && context_.input) {
        if (player_->IsDead() && context_.input->TriggerKey(DIK_R)) {
            player_->Respawn();
            SpawnEnemies();

            if (context_.offscreenRenderer) {
                context_.offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
            }
        }
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

        for (auto& floorObject : floorObjects_) {
            floorObject->Update();
        }

        for (auto& wallObject : wallObjects_) {
            wallObject->Update();
        }

        if (skybox_) {
            skybox_->Update();
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

    // 全敵の視界判定後に近接攻撃を処理する
    UpdateMeleeAttack();

    CheckCollisions();

    if (canClearCurrentLevel_ && enemies_.empty()) {
        sceneManager_->SetNextScene(std::make_unique<ClearScene>());
        return;
    }

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

    // 左下に残弾UIを描画する
    DrawAmmoUiSprites();
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
    sprites_.clear();
    ammoSprites_.clear();
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

    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);
    navMeshData_ = levelData.navMesh;
    // 敵AI用のNavMeshデータを保存する
    // マップ用のモデルとコライダーを作る前に、レベル階層を平坦化する
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

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

            if (objectData.name.find("Wall") != std::string::npos ||
                objectData.name.find("wall") != std::string::npos) {
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

    // 最初から敵がいないレベルでは、読み込んだ瞬間にClearへ進まないようにする
    canClearCurrentLevel_ = !enemies_.empty();
}
