#include "GamePlayScene.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../engine/3d/model/ModelManager.h"
#include "../../engine/3d/obj3d/Object3d.h"

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

// レベルの配置データからプレイヤーの開始位置を設定する。
void GamePlayScene::ApplyPlayerSpawnFromLevelData(const std::vector<LevelObjectData>& allObjects)
{
    // 読み込み時に一度だけ平坦化した一覧から、プレイヤーの開始位置を探す。
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

// 現在のレベルを読み直し、地形や敵、プレイヤーの開始位置を更新する。
void GamePlayScene::ReloadLevel(bool isManualReload)
{
    // 初回・F5・自動更新・ステージ移動のいずれも、最新のJSONを一度だけ読む。
    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);
    const std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);
    // 地図と接続情報はシーンが所有し、この後に生成する全敵へ参照を渡す。
    navMeshData_ = std::move(levelData.navMesh);
    navMeshData_.BuildLinks();

    // 同じ読み込み結果から、配置と当たり判定をまとめて更新する。
    ApplyPlayerSpawnFromLevelData(allObjects);

    // Rebuild walls, floors, and colliders from the current level data.
    CreateMapObjects(allObjects);

    // Refresh the player collider references and return to the spawn position.
    if (player_) {
        player_->SetFloorColliders(&floorColliders_);
        player_->SetWallColliders(&wallColliders_);
        player_->Respawn();
    }

    // Recreate enemies from the current level data.
    SpawnEnemies(allObjects);
    SpawnBosses(allObjects);
    // ボスを作り直したときは、ボス戦タイムと撃破時の記録を取り直す。
    runStats_.bossSeconds = 0.0;
    completedRun_.reset();
    bossDefeatFrame_ = 0;
    // 敵を作り直した後に、課題の進み具合と置いてある弾薬を最初へ戻す。
    if (tutorialMode_) { ResetTutorial(allObjects); }

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

// 初回ロード用にJSONを読み、配置の一覧と経路探索の接続情報を準備する。
GamePlayScene::PreparedLevel GamePlayScene::ReadInitialLevel(const std::string& path)
{
    // この処理はCPU側のJSON解析だけを行い、DirectXや共有カメラには触れない。
    PreparedLevel result;
    result.level = LevelLoader::LoadFile(path);
    result.objects = FlattenAllLevelObjects(result.level);
    result.level.navMesh.BuildLinks();
    return result;
}

// 地形や設置物、当たり判定を消去して、配置をやり直せる状態にする。
void GamePlayScene::ResetMapObjects()
{
    ammoCrates_.clear();
    floorObjects_.clear();
    wallObjects_.clear();
    floorColliders_.clear();
    wallColliders_.clear();
    bossTeleports_.clear();

    // ステージ再読み込み時は発電機を未起動状態へ戻し、古い操作対象を解除する
    generators_.clear();
    generatorReinforcementSpawns_.clear();
    interactingGeneratorIndex_ = -1;
    generatorUiIndex_ = -1;
    generatorCompleteNoticeSeconds_ = 0.0;

    // 再読み込み後のドアは閉じた状態から始める
    doors_.clear();
    generatorDoorLink_ = GeneratorDoorLink{};

}

// 配置データ1件からオブジェクトを作り、必要な当たり判定やミニマップ表示を登録する。
void GamePlayScene::CreateMapObject(const LevelObjectData& objectData,
    std::vector<MinimapObjectData>& minimapObjects)
{
    // 初回の分割読み込みと、F5での再読み込みで同じ1個分の生成処理を使う。

    // 発電機より先に並んでいるEmptyも、名前で紐づけて保持する。
    if (objectData.objectKind == "generator_reinforcement") {
        if (objectData.type == "EMPTY" && !objectData.targetGenerator.empty()) {
            generatorReinforcementSpawns_[objectData.targetGenerator].push_back({ objectData.name, objectData.translation });
        }
        return;
    }

    if (objectData.type != "MESH") {
        return;
    }

    if (objectData.name.rfind("Enemy_", 0) == 0 || objectData.name.rfind("enemy_", 0) == 0) {
        return;
    }
    // PlayerモデルはPlayerクラスが管理するため、マップ側では作らない
    if (objectData.objectKind == "player" || objectData.name == "Player") {
        return;
    }


    if (objectData.fileName.empty()) {
        return;
    }

    ModelManager::GetInstance()->LoadModel(objectData.fileName);

    auto mapObject = std::make_unique<Object3d>();
    mapObject->Initialize(GetContext().object3dCommon);
    mapObject->SetModel(objectData.fileName);
    mapObject->SetScale(objectData.scaling);
    mapObject->SetRotate(objectData.rotation);
    mapObject->SetTranslate(objectData.translation);
    mapObject->Update();

    if (objectData.objectKind == "ammo_crate") {
        // 木箱は床ではなく、銃弾が当たって壊れる独立した障害物として管理する。
        const Vector3 halfSize{ std::abs(objectData.scaling.x), std::abs(objectData.scaling.y),
            std::abs(objectData.scaling.z) };
        const size_t wallIndex = wallColliders_.size();
        wallColliders_.push_back({ "BOX", objectData.translation,
            { halfSize.x * 2.0f, halfSize.y * 2.0f, halfSize.z * 2.0f }, true });
        ammoCrates_.push_back({ std::move(mapObject), { objectData.translation, halfSize, 0.0f }, wallIndex });
        return;
    }

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
        return;
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
                generator.name = objectData.name;
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
                // TutorialDoor_3 なら、3番目の課題を達成した時に開く
                constexpr const char* kTutorialDoorPrefix = "TutorialDoor_";
                if (objectData.name.rfind(kTutorialDoorPrefix, 0) == 0) {
                    door.tutorialStep = std::atoi(objectData.name.c_str() + std::strlen(kTutorialDoorPrefix));
                }
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

// 地形と設置物をまとめて作り直し、ミニマップに配置を反映する。
void GamePlayScene::CreateMapObjects(const std::vector<LevelObjectData>& allObjects)
{
    ResetMapObjects();
    std::vector<MinimapObjectData> markers;
    for (const auto& object : allObjects) { CreateMapObject(object, markers); }
    if (minimap_) { minimap_->SetObjectMarkers(markers); }
}

// 敵にこのステージの当たり判定、経路探索データ、血の演出を設定する。
void GamePlayScene::ConfigureEnemyForLevel(Enemy& enemy)
{
    // 初期配置と増援で、敵の当たり判定と経路探索に同じデータを渡す。
    enemy.SetBloodParticleSystem(bloodParticleSystem_.get());
    enemy.SetMap(&mapField_, tileSize_);
    enemy.SetFloorColliders(&floorColliders_);
    enemy.SetWallColliders(&wallColliders_);
    enemy.SetNavMesh(&navMeshData_);
}

// レベルに設定された出現位置と巡回地点を使って通常の敵を配置する。
void GamePlayScene::SpawnEnemies(const std::vector<LevelObjectData>& allObjects)
{
    // レベルデータから敵を作り直す
    enemies_.clear();
    ammoDrops_.clear(); // 別のレベルや再読み込みに回収前の弾薬を持ち越さない。

    // 敵オブジェクト名ごとに出現位置を保存する
    std::unordered_map<std::string, Vector3> enemySpawnMap;

    // 敵オブジェクト名と番号ごとにウェイポイント位置を保存する
    std::unordered_map<std::string, std::vector<std::pair<int, Vector3>>> enemyWaypointMap;

    // マップと同じ読み込み結果から敵と巡回地点を取得する。

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
    tutorialEnemyTotals_.clear();
    for (const auto& enemyEntry : enemySpawnMap) {
        const std::string& enemyName = enemyEntry.first;
        const Vector3& spawnPosition = enemyEntry.second;

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

        // JSONから読み込んだウェイポイント一覧を敵に設定して追加する
        auto enemy = CreateLevelEnemy(enemyName, spawnPosition, waypoints);
        // チュートリアルの敵は、課題ごとの最初の数を数えておく
        if (enemy->GetTutorialStep() > 0) { ++tutorialEnemyTotals_[enemy->GetTutorialStep()]; }
        enemies_.push_back(std::move(enemy));
    }
}

// レベルの配置データからボスを生成し、地形の当たり判定を設定する。
void GamePlayScene::SpawnBosses(const std::vector<LevelObjectData>& allObjects)
{
    // レベルデータからボスを作り直す
    bosses_.clear();

    // 同じ配置一覧を使い、ボスがいない通常ステージでも余分な解析をしない。

    for (const LevelObjectData& objectData : allObjects) {
        if (objectData.fileName != "boss/boss.obj") {
            continue;
        }

        auto boss = std::make_unique<Boss>();
        boss->Initialize(
            GetContext().object3dCommon,
            GetContext().camera,
            objectData.translation,
            objectData.rotation,
            objectData.scaling);

        // 巨体がステージ外へ出ないように、壁の当たり判定を共有する
        boss->SetWallColliders(&wallColliders_);
        boss->SetFloorColliders(&floorColliders_);

        bosses_.push_back(std::move(boss));
    }
}
