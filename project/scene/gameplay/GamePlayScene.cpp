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

}

void GamePlayScene::ApplyPlayerSpawnFromLevelData(const LevelData& levelData)
{
    // Flatten the hierarchy so the Player object can be found by name.
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        // Use the Player object as the spawn position source.
        if (objectData.name == "Player") {
            if (player_) {
                player_->SetSpawnPosition(objectData.translation);
            }
            return;
        }
    }
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

    // マークの中心が敵の頭上に合うように設定する
    meleeMarker_->SetAnchorPoint({ 0.5f, 0.5f });
    meleeMarker_->SetSize({ 42.0f, 42.0f });
    meleeMarker_->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f });
    meleeMarker_->Update();

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

    for (auto& floorObject : floorObjects_) {
        floorObject->Update();
    }

    for (auto& wallObject : wallObjects_) {
        wallObject->Update();
    }

    if (skybox_) {
        skybox_->Update();
    }

    for (auto& enemy : enemies_) {
        // 敵の追跡目標を現在のプレイヤー位置に更新する
        enemy->SetTargetPosition(player_->GetWorldPosition());

        // 敵の移動と状態を更新する
        enemy->Update();


        // 敵更新後に重なりを解消する
        ResolveEnemyOverlap();
    }

    // 全敵の視界判定後に近接攻撃を処理する
    UpdateMeleeAttack();

    CheckCollisions();

    if (enemies_.empty()) {
        sceneManager_->SetNextScene(std::make_unique<ClearScene>());
        return;
    }

    if (player_ && context_.offscreenRenderer) {
        if (player_->IsDead()) {
            context_.offscreenRenderer->SetPostEffectType(PostEffectType::Grayscale);
        } else {
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
}

void GamePlayScene::UpdateMeleeAttack()
{
    if (!player_ || player_->IsDead() || !context_.input) {
        return;
    }

    // キル演出中は敵の手前まで素早く踏み込む
    if (isMeleeAttacking_) {
        meleeAttackTimer_++;

        // 演出途中で敵が別の攻撃により倒された場合は中断する
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
            // 踏み込み先で敵を即死させ、その位置に残る
            player_->SetPosition(meleeStrikePosition_);
            meleeVictim_->OnMeleeHit();

            isMeleeAttacking_ = false;
            meleeVictim_ = nullptr;
            meleeAttackTimer_ = 0;
        }
        return;
    }

    // 通常時は毎フレーム攻撃対象を探し直す
    meleeTarget_ = nullptr;
    const Vector3 playerPosition = player_->GetWorldPosition();
    float nearestDistanceSq = meleeAttackRange_ * meleeAttackRange_;

    for (auto& enemy : enemies_) {
        // 死亡済み、または一度でもプレイヤーを発見した敵は対象外にする
        if (enemy->IsDead() || enemy->HasDetectedPlayer()) {
            continue;
        }

        const Vector3 enemyPosition = enemy->GetWorldPosition();
        const float diffX = enemyPosition.x - playerPosition.x;
        const float diffZ = enemyPosition.z - playerPosition.z;
        const float distanceSq = diffX * diffX + diffZ * diffZ;

        // 攻撃範囲内で一番近い敵を選ぶ
        if (distanceSq <= nearestDistanceSq) {
            nearestDistanceSq = distanceSq;
            meleeTarget_ = enemy.get();
        }
    }

    if (!meleeTarget_ || !context_.input->TriggerKey(DIK_E)) {
        return;
    }

    // Eキーを押した瞬間に敵へ踏み込むキル演出を開始する
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
    toVictim = Normalize(toVictim);

    // 敵と重ならないよう、敵の少し手前を踏み込み位置にする
    meleeStrikePosition_ = {
        victimPosition.x - toVictim.x * 1.2f,
        playerPosition.y,
        victimPosition.z - toVictim.z * 1.2f
    };
    meleeTarget_ = nullptr;
}
void GamePlayScene::Finalize()
{
    sprites_.clear();
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

    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);
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
        if (objectData.name == "Player") {
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
