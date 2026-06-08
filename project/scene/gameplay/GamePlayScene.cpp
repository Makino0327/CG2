#include "GamePlayScene.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <memory>
#include <cmath>
#include <random>

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

void GamePlayScene::Initialize()
{
    if (initialized_) { return; }
    initialized_ = true;

    // ★ すべて context_ 経由に変更。sceneManager_ は直接使用。
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

    // 3D オブジェクト共通
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(context_.object3dCommon);

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel("fence.obj");
    ModelManager::GetInstance()->LoadModel("plane.obj");
    ModelManager::GetInstance()->LoadModel("cube.obj");
    // プレイヤーモデルを読み込む
    ModelManager::GetInstance()->LoadModel("player/player.obj");
    // プレイヤー弾モデルを読み込む
    ModelManager::GetInstance()->LoadModel("bullet/bullet.obj");
    // 敵モデルを読み込む
    ModelManager::GetInstance()->LoadModel("enemy/enemy.obj");
    // マップの床と壁の仮表示に使う block モデルを読み込む
    ModelManager::GetInstance()->LoadModel("block/block.obj");

    // テクスチャ
    auto texMan = TextureManager::GetInstance();
    texMan->LoadTexture("Resources/uvChecker.png");
    texMan->LoadTexture("Resources/monsterBall.png");
    texMan->LoadTexture("Resources/checkerBoard.png");
    texMan->LoadTexture("Resources/circle2.png");
    texMan->LoadTexture("Resources/fence.png");
    texMan->LoadTexture("Resources/Cube.png");
    texMan->LoadTexture("Resources/skybox.dds");
    texMan->LoadTexture("Resources/gradationLine.png"); // // Ring 用のグラデーションテクスチャ

    // Material
    materialResource_ = context_.dxCommon->CreateBufferResource(sizeof(Material));
    Material* materialData = nullptr;
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->lightingType = 0;
    materialData->environmentCoefficient = 0.0f;
    materialData->uvTransform = MakeIdentity4x4();

    // DirectionalLight
    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    light->direction = Vector3(0.0f, -1.0f, 0.0f);
    light->intensity = 4.0f;

	// Skybox
    skyboxCommon_ = std::make_unique<SkyboxCommon>();
    skyboxCommon_->Initialize(context_.dxCommon, context_.srvManager);
    skyboxCommon_->SetDefaultCamera(context_.camera);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(skyboxCommon_.get());
    skybox_->SetCamera(context_.camera);

    // デバッグ画面でカメラを操作するクラスを生成する
    debugCamera_ = std::make_unique<DebugCamera>();

    /// プレイヤー
    player_ = std::make_unique<Player>();
    // プレイヤーを初期化する
    player_->Initialize(context_.object3dCommon, context_.input);

    /// カメラ
    // プレイヤー追従カメラを作る
    followCamera_ = std::make_unique<FollowCamera>();
    // シーンで使うカメラを追従カメラに渡す
    followCamera_->Initialize(context_.camera);
    // 初期ターゲットをプレイヤー位置にしておく
    followCamera_->SetTarget(player_->GetWorldPosition());

    
    // シーン開始時は通常表示に戻しておく
    if (context_.offscreenRenderer) {
        context_.offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
    }

    /// マップ
    // マップCSVを読み込む
    mapField_.LoadFromCsv("Resources/map.csv");

    // プレイヤーにマップ情報と1マスの大きさを渡す
    player_->SetMap(&mapField_, tileSize_);

    // 敵をマップ内の空きマスに生成する
    SpawnEnemies();

    // マップから床と壁のオブジェクトを作る
    CreateMapObjects();

    // プレイヤーに Blender JSON の床コライダー一覧を渡す
    player_->SetFloorColliders(&floorColliders_);

    // プレイヤーに Blender JSON の壁コライダー一覧を渡す
    player_->SetWallColliders(&wallColliders_);
}

void GamePlayScene::Update()
{
   
    const float dt = 1.0f / 60.0f;

    // プレイヤー死亡中にRが押されたら敵も含めて再生成する
    if (player_ && context_.input) {
        if (player_->IsDead() && context_.input->TriggerKey(DIK_R)) {
            // プレイヤーを復活させる
            player_->Respawn();

            // 敵を再生成する
            SpawnEnemies();

            // 画面効果を通常に戻す
            if (context_.offscreenRenderer) {
                context_.offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
            }
        }
    }

    // ★ context_ 経由に変更
    if (skybox_) { skybox_->Update(); }


    // Particleを毎フレーム更新する
    if (particleSystem_) { particleSystem_->Update(dt); }

       // デバッグ用のカメラ操作を更新する
    if (debugCamera_ && context_.isDebugMode) {
        debugCamera_->Update(
            context_.camera,
            context_.input,
            context_.offscreenRenderer,
            *context_.isDebugMode);
    }

    // // F1中はゲーム進行を止めるが、カメラと描画行列だけは更新する
    if (context_.isDebugMode && *context_.isDebugMode) {
        // // デバッグカメラで変更した transform から ViewProjection を更新する
        if (context_.camera) {
            context_.camera->Update();
        }

        // // プレイヤーの見た目だけ更新する
        if (player_) {
            player_->UpdateRenderOnly();
        }

        // // 敵の見た目だけ更新する
        for (auto& enemy : enemies_) {
            enemy->UpdateRenderOnly();
        }

        // // 床と壁の見た目だけ更新する
        for (auto& floorObject : floorObjects_) {
            floorObject->Update();
        }

        for (auto& wallObject : wallObjects_) {
            wallObject->Update();
        }

        // // Skybox もカメラ反映のため更新する
        if (skybox_) {
            skybox_->Update();
        }

        return;
    }

    /// =============================
    /// プレイヤー
    /// =============================
    // プレイヤーを更新する
    if (player_) {
        player_->Update(context_.camera);
    }
    if (context_.camera) { context_.camera->Update(); }

    // プレイヤー座標を追従カメラへ渡して更新する
    if (player_ && followCamera_) {
        followCamera_->SetTarget(player_->GetWorldPosition());
        followCamera_->Update();
    }

    // カメラ更新後の行列を反映するため床を毎フレーム更新する
    for (auto& floorObject : floorObjects_) {
        floorObject->Update();
    }

    // カメラ更新後の行列を反映するため壁を毎フレーム更新する
    for (auto& wallObject : wallObjects_) {
        wallObject->Update();
    }

    // カメラ更新後の行列でSkyboxを更新する
    if (skybox_) {
        skybox_->Update();
    }

    /// =============================
    /// 敵
    /// =============================
    // 敵を更新する
    for (auto& enemy : enemies_) {
        // 敵にプレイヤーの位置を渡す
        enemy->SetTargetPosition(player_->GetWorldPosition());

        // 敵を更新する
        enemy->Update();

        // 敵同士の重なりを解消する
        ResolveEnemyOverlap();

    }


    // プレイヤーと敵と弾の当たり判定を処理する
    CheckCollisions();

    // 敵を全部倒したらクリアへ切り替える
    if (enemies_.empty()) {
        sceneManager_->SetNextScene(std::make_unique<ClearScene>());
        return;
    }

    // 死亡中はグレイスケール、生存中は通常表示にする
    if (player_ && context_.offscreenRenderer) {
        if (player_->IsDead()) {
            // 死亡中はゲーム画面をグレイスケールにする
            context_.offscreenRenderer->SetPostEffectType(PostEffectType::Grayscale);
        } else {
            // Rで復活した後は通常表示に戻す
            context_.offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        }
    }
}

void GamePlayScene::Draw()
{
    // ★ context_ 経由に変更
    assert(context_.dxCommon);
    assert(context_.spriteCommon);
    assert(context_.object3dCommon);
    assert(context_.particleCommon);

    ID3D12GraphicsCommandList* commandList = context_.dxCommon->GetCommandList();
    assert(commandList);

    // Sprite
    context_.spriteCommon->CommonDrawSetting();

    if (skybox_) {
        skybox_->Draw();
    }

    // Particle
    context_.particleCommon->CommonDrawSetting();

    // Particleを描画する
    if (particleSystem_) { particleSystem_->Draw(); }
    
    // プレイヤーを描画する
    if (player_) {
        player_->Draw();
    }

    // 敵をまとめて描画する
    for (auto& enemy : enemies_) {
        enemy->Draw();
    }

    // 床をまとめて描画する
    for (auto& floorObject : floorObjects_) {
        floorObject->Draw();
    }
    // 壁をまとめて描画する
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

    // プレイヤーを解放する
    player_.reset();
    // 敵を全て解放する
    enemies_.clear();

}

void GamePlayScene::CheckCollisions()
{
    if (!player_) {
        return;
    }

    // プレイヤーと敵の当たり判定を処理する
    for (auto& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        if (Collision::IsHit(player_->GetCollider(), enemy->GetCollider())) {
            // プレイヤーに敵が触れたことを通知する
            player_->OnHit();
        }
    }

    // プレイヤーが持っている弾と敵の当たり判定を処理する
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
                // 当たった弾を消す
                bullet->OnHit();

                // 当たった敵を倒す
                enemy->OnHit();
                break;
            }
        }
    }

    // 倒された敵を配列から取り除く
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

    // ポストエフェクト
    // オフスクリーン描画結果をデバッグ表示する
    if (context_.offscreenRenderer) {
        context_.offscreenRenderer->DrawDebugGameViewImGui();
        context_.offscreenRenderer->DrawImGui();
    }

#endif
}

void GamePlayScene::ResolveEnemyOverlap()
{
    // 全敵の組み合わせを調べる
    for (size_t i = 0; i < enemies_.size(); ++i) {
        for (size_t j = i + 1; j < enemies_.size(); ++j) {
            Vector3 posA = enemies_[i]->GetWorldPosition();
            Vector3 posB = enemies_[j]->GetWorldPosition();

            // 敵Aから敵Bへの差分を作る
            Vector3 diff = {
                posB.x - posA.x,
                0.0f,
                posB.z - posA.z
            };

            float distanceSq = diff.x * diff.x + diff.z * diff.z;
            float radiusSum = enemies_[i]->GetBodyRadius() + enemies_[j]->GetBodyRadius();

            // 完全に同じ位置だと正規化できないので少しずらす
            if (distanceSq <= 0.0001f) {
                diff = { 1.0f, 0.0f, 0.0f };
                distanceSq = 1.0f;
            }

            float distance = std::sqrt(distanceSq);

            // 重なっている時だけ押し戻す
            if (distance < radiusSum) {
                float overlap = radiusSum - distance;

                // 押し戻す方向を正規化する
                Vector3 push = {
                    diff.x / distance,
                    0.0f,
                    diff.z / distance
                };

                // 互いに半分ずつ離す
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
    // 既存の床と壁の描画用オブジェクトを最初に空にする
    floorObjects_.clear();
    wallObjects_.clear();
    floorColliders_.clear();
    wallColliders_.clear();

    // Blender から出力した JSON を読み込む
    LevelData levelData = LevelLoader::LoadFile("Resources/level/testScene.json");

    // JSON 内のオブジェクトを順番に確認して床として生成する
    for (const LevelObjectData& objectData : levelData.objects) {
        // MESH 以外は使わない
        if (objectData.type != "MESH") {
            continue;
        }

        // モデル名が空なら生成しない
        if (objectData.fileName.empty()) {
            continue;
        }

        // JSON に書かれたモデルを事前に読み込む
        ModelManager::GetInstance()->LoadModel(objectData.fileName);

        auto mapObject = std::make_unique<Object3d>();

        // 3D オブジェクトを初期化する
        mapObject->Initialize(context_.object3dCommon);

        // Blender JSON に書かれた情報を使う
        mapObject->SetModel(objectData.fileName);
        mapObject->SetScale(objectData.scaling);
        mapObject->SetRotate(objectData.rotation);
        mapObject->SetTranslate(objectData.translation);
        mapObject->Update();

        // 今回は回転なしの BOX collider 前提でワールド AABB を作る
        if (objectData.collider.hasCollider && objectData.collider.type == "BOX") {
            LevelColliderData worldCollider = objectData.collider;

            // collider.center はローカル座標なので平行移動を足す
            worldCollider.center.x =
                objectData.translation.x + objectData.collider.center.x * objectData.scaling.x;
            worldCollider.center.y =
                objectData.translation.y + objectData.collider.center.y * objectData.scaling.y;
            worldCollider.center.z =
                objectData.translation.z + objectData.collider.center.z * objectData.scaling.z;

            // collider.size にオブジェクトの拡大率を掛けてワールドサイズにする
            worldCollider.size.x = objectData.collider.size.x * objectData.scaling.x;
            worldCollider.size.y = objectData.collider.size.y * objectData.scaling.y;
            worldCollider.size.z = objectData.collider.size.z * objectData.scaling.z;

            // // 名前に Wall / wall が入っているものは壁として扱う
            if (objectData.name.find("Wall") != std::string::npos ||
                objectData.name.find("wall") != std::string::npos) {
                // // 壁コライダーを保存する
                wallColliders_.push_back(worldCollider);

                // // 壁オブジェクトとして描画用リストへ入れる
                wallObjects_.push_back(std::move(mapObject));
            } else {
                // // 床コライダーを保存する
                floorColliders_.push_back(worldCollider);

                // // 床オブジェクトとして描画用リストへ入れる
                floorObjects_.push_back(std::move(mapObject));
            }
        } else {
            // collider が無いものは見た目だけ床側に入れておく
            floorObjects_.push_back(std::move(mapObject));
        }
    }
}

void GamePlayScene::SpawnEnemies()
{
    // ランダム生成用の乱数を用意する
    std::random_device seedGenerator;
    std::mt19937 randomEngine(seedGenerator());

    // 敵を置ける空きマス一覧を作る
    std::vector<Vector3> spawnCandidates;

    for (int z = 0; z < mapField_.GetHeight(); ++z) {
        for (int x = 0; x < mapField_.GetWidth(); ++x) {
            // 空きマスだけ候補にする
            if (mapField_.GetChip(x, z) != MapChipType::Empty) {
                continue;
            }

            Vector3 spawnPosition = {
                static_cast<float>(x) * tileSize_,
                0.5f,
                static_cast<float>(mapField_.GetHeight() - 1 - z) * tileSize_
            };

            // プレイヤーの位置に近すぎる候補は除外する
            Vector3 diff = {
                spawnPosition.x - player_->GetWorldPosition().x,
                0.0f,
                spawnPosition.z - player_->GetWorldPosition().z
            };

            float distanceSq = diff.x * diff.x + diff.z * diff.z;
            if (distanceSq < 25.0f) {
                continue;
            }

            spawnCandidates.push_back(spawnPosition);
        }
    }

    // 候補をシャッフルして先頭から使う
    std::shuffle(spawnCandidates.begin(), spawnCandidates.end(), randomEngine);

    // いったん今の敵を消す
    enemies_.clear();

    // 候補が足りない時はある分だけ生成する
    uint32_t spawnCount = std::min(
        enemyCount_,
        static_cast<uint32_t>(spawnCandidates.size()));

    for (uint32_t i = 0; i < spawnCount; ++i) {
        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(context_.object3dCommon, spawnCandidates[i]);

        // 敵にもマップ情報を渡す
        enemy->SetMap(&mapField_, tileSize_);

        // 敵に Blender JSON の床コライダー一覧を渡す
        enemy->SetFloorColliders(&floorColliders_);

        // 敵に Blender JSON の壁コライダー一覧を渡す
        enemy->SetWallColliders(&wallColliders_);

        enemies_.push_back(std::move(enemy));
    }
}