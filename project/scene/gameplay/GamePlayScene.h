#pragma once
#define NOMINMAX
#include <vector>
#include <string>
#include <memory>
#include <wrl.h>
#include <d3d12.h>

#include "../engine/math/Math.h"
#include "../BaseScene.h"
#include "../engine/audio/SoundManager.h"
#include "../engine/3d/skybox/Skybox.h"
#include "../engine/3d/skybox/SkyboxCommon.h"
#include "../engine/animation/Animation.h"
#include "../../engine/3d/obj3d/Line3DCommon.h"
#include "../../engine/3d/obj3d/DebugSkeletonRenderer.h"
#include "../../game/camera/DebugCamera.h"
#include "../../game/player/Player.h"
#include "../../game/camera/FollowCamera.h"
#include "../../game/enemy/Enemy.h"
#include "../../game/map/MapChipField.h"

class Sprite;
class Object3d;
class ParticleSystem;
class Skybox;
class SkyboxCommon;

//==================================================
// GamePlayScene
//==================================================
class GamePlayScene : public BaseScene
{
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void DrawImGui() override;

private:
    // プレイヤーと敵と弾の当たり判定をまとめて処理する
    void CheckCollisions();
    // 敵同士の重なりを解消する
    void ResolveEnemyOverlap();
    // マップCSVから床と壁のオブジェクトを作る
    void CreateMapObjects();
    // 敵をマップ内の空きマスに再生成する
    void SpawnEnemies();
private:
    std::unique_ptr<Object3d> object3d_;

    std::unique_ptr<SkyboxCommon> skyboxCommon_;
    std::unique_ptr<Skybox> skybox_;

    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    bool initialized_ = false;

    std::unique_ptr<DebugCamera> debugCamera_;
    std::unique_ptr<FollowCamera> followCamera_;

private:
    /// プレイヤー
    std::unique_ptr<Player> player_;
    Vector3 playerTranslate_ = { 2.0f, 0.5f, 0.0f };
    Vector3 playerRotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 playerScale_ = { 1.0f, 1.0f, 1.0f };

    /// 敵
    // 敵を複数管理する
    std::vector<std::unique_ptr<Enemy>> enemies_;
    // 出現させる敵の数
    uint32_t enemyCount_ = 10;
    // プレイヤーから敵を配置する半径
    float enemySpawnRadius_ = 8.0f;

    /// マップ
    // マップCSVの内容を保持する
    MapChipField mapField_;
    // 1マス分の大きさ
    float tileSize_ = 2.0f;
    // 床マスの描画用オブジェクトを持つ
    std::vector<std::unique_ptr<Object3d>> floorObjects_;
    // 壁マスの描画用オブジェクトを持つ
    std::vector<std::unique_ptr<Object3d>> wallObjects_;

    // 被弾時の赤ビネットを表示する残りフレーム
    int damageVignetteTimer_ = 0;

    // 赤ビネットを表示するフレーム数
    int damageVignetteDuration_ = 12;
};
