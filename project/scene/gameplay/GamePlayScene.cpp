#include "GamePlayScene.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <memory>
#include <cmath>
#include <random>
#include <unordered_map>

#include "../engine/base/directX/DirectXCommon.h"
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

    // 親子込みのワールド座標へ変換したオブジェクト一覧を作る
    void FlattenLevelObjectsRecursive(
        const LevelObjectData& objectData,
        const Vector3& parentTranslation,
        std::vector<LevelObjectData>& outObjects)
    {
        // 今のオブジェクトをコピーする
        LevelObjectData worldObject = objectData;

        // 親の座標を足してワールド座標にする
        worldObject.translation.x += parentTranslation.x;
        worldObject.translation.y += parentTranslation.y;
        worldObject.translation.z += parentTranslation.z;

        // 子配列はここでは使わないので空にしておく
        worldObject.children.clear();

        // 平坦化した一覧へ追加する
        outObjects.push_back(worldObject);

        // 子オブジェクトも親座標を引き継いで再帰する
        for (const LevelObjectData& child : objectData.children) {
            FlattenLevelObjectsRecursive(child, worldObject.translation, outObjects);
        }
    }

    // レベル全体を親子込みのワールド座標オブジェクト一覧へ変換する
    std::vector<LevelObjectData> FlattenAllLevelObjects(const LevelData& levelData)
    {
        std::vector<LevelObjectData> allObjects;

        for (const LevelObjectData& objectData : levelData.objects) {
            FlattenLevelObjectsRecursive(objectData, Vector3{ 0.0f, 0.0f, 0.0f }, allObjects);
        }

        return allObjects;
    }

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

    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(context_.dxCommon, context_.srvManager);
    skyboxCommon_->SetDefaultCamera(context_.camera);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(skyboxCommon_.get());
    skybox_->SetCamera(context_.camera);

    debugCamera_ = std::make_unique<DebugCamera>();

    player_ = std::make_unique<Player>();

    // Blender の Player オブジェクトを探して開始位置に使う
    {
        LevelData levelData = LevelLoader::LoadFile("Resources/level/testScene.json");
        std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

        for (const LevelObjectData& objectData : allObjects) {
            // 名前が Player のオブジェクトを開始位置として使う
            if (objectData.name == "Player") {
                player_->SetSpawnPosition(objectData.translation);
                break;
            }
        }
    }

    player_->Initialize(context_.object3dCommon, context_.input);

    followCamera_ = std::make_unique<FollowCamera>();
    followCamera_->Initialize(context_.camera);
    followCamera_->SetTarget(player_->GetWorldPosition());

    if (context_.offscreenRenderer) {
        // 常に画面全体へアウトラインをかける
        context_.offscreenRenderer->SetPostEffectType(PostEffectType::DepthOutline);
    }

    mapField_.LoadFromCsv("Resources/map.csv");
    player_->SetMap(&mapField_, tileSize_);

    CreateMapObjects();
    player_->SetFloorColliders(&floorColliders_);
    player_->SetWallColliders(&wallColliders_);
    SpawnEnemies();
}

void GamePlayScene::Update()
{
    const float dt = 1.0f / 60.0f;

    if (player_ && context_.input) {
        if (player_->IsDead() && context_.input->TriggerKey(DIK_R)) {
            player_->Respawn();
            SpawnEnemies();

            if (context_.offscreenRenderer) {
                // リスポーン後も画面全体へアウトラインをかける
                context_.offscreenRenderer->SetPostEffectType(PostEffectType::DepthOutline);
            }
        }
    }

    if (skybox_) { skybox_->Update(); }
    if (particleSystem_) { particleSystem_->Update(dt); }

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

        return;
    }

    if (player_) {
        player_->Update(context_.camera);
    }
    if (context_.camera) { context_.camera->Update(); }

    if (player_ && followCamera_) {
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
        // 毎フレーム敵にプレイヤーの現在位置を渡す
        enemy->SetTargetPosition(player_->GetWorldPosition());

        // 敵の更新を行う
        enemy->Update();

        // 敵同士が重ならないように補正する
        ResolveEnemyOverlap();
    }

    CheckCollisions();

    if (enemies_.empty()) {
        sceneManager_->SetNextScene(std::make_unique<ClearScene>());
        return;
    }

    if (player_ && context_.offscreenRenderer) {
        if (player_->IsDead()) {
            // 死亡中だけグレースケールをかける
            context_.offscreenRenderer->SetPostEffectType(PostEffectType::Grayscale);
        } else {
            // 生存中は常に画面全体へアウトラインをかける
            context_.offscreenRenderer->SetPostEffectType(PostEffectType::DepthOutline);
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

    context_.particleCommon->CommonDrawSetting();

    if (particleSystem_) { particleSystem_->Draw(); }

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
}

void GamePlayScene::Finalize()
{
    sprites_.clear();

    object3d_.reset();
    particleSystem_.reset();
    skybox_.reset();
    skyboxCommon_.reset();

    materialResource_.Reset();
    directionalLightResource_.Reset();

    initialized_ = false;

    debugCamera_.reset();
    player_.reset();
    enemies_.clear();
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

        if (Collision::IsHit(player_->GetCollider(), enemy->GetCollider())) {
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
                bullet->OnHit();
                enemy->OnHit();
                break;
            }
        }
    }

    enemies_.erase(
        std::remove_if(
            enemies_.begin(),
            enemies_.end(),
            [](const std::unique_ptr<Enemy>& enemy) {
                return enemy->IsDead();
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

    LevelData levelData = LevelLoader::LoadFile("Resources/level/testScene.json");
    // 親子込みのワールド座標オブジェクト一覧を作る
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        if (objectData.type != "MESH") {
            continue;
        }

        if (objectData.name.rfind("Enemy_", 0) == 0 || objectData.name.rfind("enemy_", 0) == 0) {
            continue;
        }
        // Player は開始位置用なのでマップオブジェクトとしては描画しない
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
    // 既存の敵を消す
    enemies_.clear();

    // Blender から出力した JSON を読み込む
    LevelData levelData = LevelLoader::LoadFile("Resources/level/testScene.json");

    // 敵の初期位置を名前ごとに保持する
    std::unordered_map<std::string, Vector3> enemySpawnMap;

    // 敵ごとの waypoint を番号付きで保持する
    std::unordered_map<std::string, std::vector<std::pair<int, Vector3>>> enemyWaypointMap;

    // 親子込みのワールド座標オブジェクト一覧を作る
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        const std::string& name = objectData.name;

        // Enemy_00_Waypoint_00 形式の経路点を集める
        size_t waypointPos = name.find("_Waypoint_");
        if (waypointPos != std::string::npos) {
            std::string enemyName = name.substr(0, waypointPos);
            std::string indexText = name.substr(waypointPos + std::string("_Waypoint_").size());
            int waypointIndex = std::stoi(indexText);

            // waypoint のワールド座標を保存する
            enemyWaypointMap[enemyName].push_back({ waypointIndex, objectData.translation });
            continue;
        }

        // Enemy_ で始まるものを敵の初期位置として使う
        if (name.rfind("Enemy_", 0) == 0 || name.rfind("enemy_", 0) == 0) {
            // 敵のワールド初期位置を保存する
            enemySpawnMap[name] = objectData.translation;
        }
    }

    // JSON 上の敵定義から敵を生成する
    for (const auto& enemyEntry : enemySpawnMap) {
        const std::string& enemyName = enemyEntry.first;
        const Vector3& spawnPosition = enemyEntry.second;

        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(context_.object3dCommon, context_.camera, spawnPosition);

        // 既存の参照もそのまま渡しておく
        enemy->SetMap(&mapField_, tileSize_);
        enemy->SetFloorColliders(&floorColliders_);
        enemy->SetWallColliders(&wallColliders_);

        // waypoint を番号順に並べる
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

        // JSON の経路点を敵へ渡す
        enemy->SetWaypoints(waypoints);

        enemies_.push_back(std::move(enemy));
    }
}
