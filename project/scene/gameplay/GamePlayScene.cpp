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

    // 髫包ｽｪ陝・頃・ｾ・ｼ邵ｺ・ｿ邵ｺ・ｮ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥・ｺ・ｧ隶灘生竏郁棔逕ｻ驪､邵ｺ蜉ｱ笳・ｹｧ・ｪ郢晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢昜ｺ包ｽｸﾂ髫包ｽｧ郢ｧ蜑・ｽｽ諛奇ｽ・
    void FlattenLevelObjectsRecursive(
        const LevelObjectData& objectData,
        const Vector3& parentTranslation,
        std::vector<LevelObjectData>& outObjects)
    {
        // 闔臥ｿｫ繝ｻ郢ｧ・ｪ郢晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢晏現・堤ｹｧ・ｳ郢晄鱒繝ｻ邵ｺ蜷ｶ・・
        LevelObjectData worldObject = objectData;

        // 髫包ｽｪ邵ｺ・ｮ陟趣ｽｧ隶灘生・帝屆・ｳ邵ｺ蜉ｱ窶ｻ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥・ｺ・ｧ隶灘生竊鍋ｸｺ蜷ｶ・・
        worldObject.translation.x += parentTranslation.x;
        worldObject.translation.y += parentTranslation.y;
        worldObject.translation.z += parentTranslation.z;

        // 陝・ｮ｣繝ｻ陋ｻ蜉ｱ繝ｻ邵ｺ阮呻ｼ・ｸｺ・ｧ邵ｺ・ｯ闖ｴ・ｿ郢ｧ荳岩・邵ｺ繝ｻ繝ｻ邵ｺ・ｧ驕ｨ・ｺ邵ｺ・ｫ邵ｺ蜉ｱ窶ｻ邵ｺ鄙ｫ・･
        worldObject.children.clear();

        // 陝ｷ・ｳ陜ｮ・ｦ陋ｹ謔ｶ・邵ｺ貊会ｽｸﾂ髫包ｽｧ邵ｺ・ｸ髴托ｽｽ陷会｣ｰ邵ｺ蜷ｶ・・
        outObjects.push_back(worldObject);

        // 陝・・縺檎ｹ晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢晏現・る囎・ｪ陟趣ｽｧ隶灘生・定題ｼ披ｳ驍ｯ蜷ｶ・樒ｸｺ・ｧ陷讎奇ｽｸ・ｰ邵ｺ蜷ｶ・・
        for (const LevelObjectData& child : objectData.children) {
            FlattenLevelObjectsRecursive(child, worldObject.translation, outObjects);
        }
    }

    // 郢晢ｽｬ郢晏生ﾎ晁怦・ｨ闖ｴ阮呻ｽ帝囎・ｪ陝・頃・ｾ・ｼ邵ｺ・ｿ邵ｺ・ｮ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥・ｺ・ｧ隶灘生縺檎ｹ晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢昜ｺ包ｽｸﾂ髫包ｽｧ邵ｺ・ｸ陞溽判驪､邵ｺ蜷ｶ・・
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

    // 隰ｨ・ｵ邵ｺ・ｮ髫募､懊鎖陷ｿ・ｯ髫暮摩蝟ｧ邵ｺ・ｫ闖ｴ・ｿ邵ｺ繝ｻ・ｷ螢ｽ邱帝包ｽｻ郢ｧ雋槭・隴帶ｺｷ蝟ｧ邵ｺ蜷ｶ・・
    line3dCommon_ = std::make_unique<Line3DCommon>();
    line3dCommon_->Initialize(context_.dxCommon, context_.srvManager);
    enemyVisionDebug_.Initialize(context_.dxCommon, line3dCommon_.get());

    debugCamera_ = std::make_unique<DebugCamera>();

    player_ = std::make_unique<Player>();

    // Blender 邵ｺ・ｮ Player 郢ｧ・ｪ郢晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢晏現・定ｬ暦ｽ｢邵ｺ蜉ｱ窶ｻ鬮｢蜿･・ｧ蛟ｶ・ｽ蜥ｲ・ｽ・ｮ邵ｺ・ｫ闖ｴ・ｿ邵ｺ繝ｻ
    {
        LevelData levelData = LevelLoader::LoadFile("Resources/level/testScene.json");
        std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

        for (const LevelObjectData& objectData : allObjects) {
            // 陷ｷ讎顔√邵ｺ繝ｻPlayer 邵ｺ・ｮ郢ｧ・ｪ郢晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢晏現・帝ｫ｢蜿･・ｧ蛟ｶ・ｽ蜥ｲ・ｽ・ｮ邵ｺ・ｨ邵ｺ蜉ｱ窶ｻ闖ｴ・ｿ邵ｺ繝ｻ
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
        // 雎亥ｼｱ繝ｵ郢晢ｽｬ郢晢ｽｼ郢晢｣ｰ隰ｨ・ｵ邵ｺ・ｫ郢晏干ﾎ樒ｹｧ・､郢晢ｽ､郢晢ｽｼ邵ｺ・ｮ霑ｴ・ｾ陜ｨ・ｨ闖ｴ蜥ｲ・ｽ・ｮ郢ｧ蜻茨ｽｸ・｡邵ｺ繝ｻ
        enemy->SetTargetPosition(player_->GetWorldPosition());

        // 隰ｨ・ｵ邵ｺ・ｮ隴厄ｽｴ隴・ｽｰ郢ｧ螳夲ｽ｡蠕娯鴬
        enemy->Update();

        // 隰ｨ・ｵ陷ｷ謔滂ｽ｣・ｫ邵ｺ遒√裟邵ｺ・ｪ郢ｧ蟲ｨ竊醍ｸｺ繝ｻ・育ｸｺ繝ｻ竊馴勳諛茨ｽｭ・｣邵ｺ蜷ｶ・・
        ResolveEnemyOverlap();
    }

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

    // 隰ｨ・ｵ邵ｺ・ｮ髫募､懊鎖郢ｧ蛛ｵ繝ｧ郢晁・繝｣郢ｧ・ｰ驍ｱ螢ｹ縲定愾・ｯ髫暮摩蝟ｧ邵ｺ蜷ｶ・・
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
}

void GamePlayScene::Finalize()
{
    sprites_.clear();

    object3d_.reset();
    particleSystem_.reset();
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
    // 髫包ｽｪ陝・頃・ｾ・ｼ邵ｺ・ｿ邵ｺ・ｮ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥・ｺ・ｧ隶灘生縺檎ｹ晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢昜ｺ包ｽｸﾂ髫包ｽｧ郢ｧ蜑・ｽｽ諛奇ｽ・
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        if (objectData.type != "MESH") {
            continue;
        }

        if (objectData.name.rfind("Enemy_", 0) == 0 || objectData.name.rfind("enemy_", 0) == 0) {
            continue;
        }
        // Player 邵ｺ・ｯ鬮｢蜿･・ｧ蛟ｶ・ｽ蜥ｲ・ｽ・ｮ騾包ｽｨ邵ｺ・ｪ邵ｺ・ｮ邵ｺ・ｧ郢晄ｧｭ繝｣郢晏干縺檎ｹ晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢晏現竊堤ｸｺ蜉ｱ窶ｻ邵ｺ・ｯ隰蜀怜愛邵ｺ蜉ｱ竊醍ｸｺ繝ｻ
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
    // 隴鯉ｽ｢陝・･繝ｻ隰ｨ・ｵ郢ｧ蜻茨ｽｶ蛹ｻ笘・
    enemies_.clear();

    // Blender 邵ｺ荵晢ｽ芽怎・ｺ陷牙ｸ呻ｼ邵ｺ繝ｻJSON 郢ｧ螳夲ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ
    LevelData levelData = LevelLoader::LoadFile(levelFilePath_);

    // 隰ｨ・ｵ邵ｺ・ｮ陋ｻ譎・ｄ闖ｴ蜥ｲ・ｽ・ｮ郢ｧ雋樣倹陷鷹亂・・ｸｺ・ｨ邵ｺ・ｫ闖ｫ譎・亜邵ｺ蜷ｶ・・
    std::unordered_map<std::string, Vector3> enemySpawnMap;

    // 隰ｨ・ｵ邵ｺ譁絶・邵ｺ・ｮ waypoint 郢ｧ蝣､蛻・愾・ｷ闔牙･窶ｳ邵ｺ・ｧ闖ｫ譎・亜邵ｺ蜷ｶ・・
    std::unordered_map<std::string, std::vector<std::pair<int, Vector3>>> enemyWaypointMap;

    // 髫包ｽｪ陝・頃・ｾ・ｼ邵ｺ・ｿ邵ｺ・ｮ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥・ｺ・ｧ隶灘生縺檎ｹ晄じ縺夂ｹｧ・ｧ郢ｧ・ｯ郢昜ｺ包ｽｸﾂ髫包ｽｧ郢ｧ蜑・ｽｽ諛奇ｽ・
    std::vector<LevelObjectData> allObjects = FlattenAllLevelObjects(levelData);

    for (const LevelObjectData& objectData : allObjects) {
        const std::string& name = objectData.name;

        // Enemy_00_Waypoint_00 陟厄ｽ｢陟台ｸ翫・驍ｨ迹夲ｽｷ・ｯ霓､・ｹ郢ｧ蟶晏ｯ皮ｹｧ竏夲ｽ・
        size_t waypointPos = name.find("_Waypoint_");
        if (waypointPos != std::string::npos) {
            std::string enemyName = name.substr(0, waypointPos);
            std::string indexText = name.substr(waypointPos + std::string("_Waypoint_").size());
            int waypointIndex = std::stoi(indexText);

            // waypoint 邵ｺ・ｮ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥・ｺ・ｧ隶灘生・定将譎擾ｽｭ蛟･笘・ｹｧ繝ｻ
            enemyWaypointMap[enemyName].push_back({ waypointIndex, objectData.translation });
            continue;
        }

        // Enemy_ 邵ｺ・ｧ陝倶ｹ昶穐郢ｧ荵晢ｽらｸｺ・ｮ郢ｧ蜻磯峅邵ｺ・ｮ陋ｻ譎・ｄ闖ｴ蜥ｲ・ｽ・ｮ邵ｺ・ｨ邵ｺ蜉ｱ窶ｻ闖ｴ・ｿ邵ｺ繝ｻ
        if (name.rfind("Enemy_", 0) == 0 || name.rfind("enemy_", 0) == 0) {
            // 隰ｨ・ｵ邵ｺ・ｮ郢晢ｽｯ郢晢ｽｼ郢晢ｽｫ郢晉甥繝ｻ隴帶ｻ会ｽｽ蜥ｲ・ｽ・ｮ郢ｧ蜑・ｽｿ譎擾ｽｭ蛟･笘・ｹｧ繝ｻ
            enemySpawnMap[name] = objectData.translation;
        }
    }

    // JSON 闕ｳ鄙ｫ繝ｻ隰ｨ・ｵ陞ｳ螟ゑｽｾ・ｩ邵ｺ荵晢ｽ芽ｬｨ・ｵ郢ｧ蝣､蜃ｽ隰瑚・笘・ｹｧ繝ｻ
    for (const auto& enemyEntry : enemySpawnMap) {
        const std::string& enemyName = enemyEntry.first;
        const Vector3& spawnPosition = enemyEntry.second;

        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(context_.object3dCommon, context_.camera, spawnPosition);

        // 隴鯉ｽ｢陝・･繝ｻ陷ｿ繧峨・郢ｧ繧・落邵ｺ・ｮ邵ｺ・ｾ邵ｺ・ｾ雋ゑｽ｡邵ｺ蜉ｱ窶ｻ邵ｺ鄙ｫ・･
        enemy->SetMap(&mapField_, tileSize_);
        enemy->SetFloorColliders(&floorColliders_);
        enemy->SetWallColliders(&wallColliders_);

        // waypoint 郢ｧ蝣､蛻・愾・ｷ鬯・・竊楢叉・ｦ邵ｺ・ｹ郢ｧ繝ｻ
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

        // JSON 邵ｺ・ｮ驍ｨ迹夲ｽｷ・ｯ霓､・ｹ郢ｧ蜻磯峅邵ｺ・ｸ雋ゑｽ｡邵ｺ繝ｻ
        enemy->SetWaypoints(waypoints);

        enemies_.push_back(std::move(enemy));
    }
}
