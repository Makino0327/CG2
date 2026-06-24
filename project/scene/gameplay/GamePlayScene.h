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
#include "../../engine/3d/obj3d/DebugLine3D.h"
#include "../../engine/3d/obj3d/DebugSkeletonRenderer.h"
#include "../../game/camera/DebugCamera.h"
#include "../../game/player/Player.h"
#include "../../game/camera/FollowCamera.h"
#include "../../game/enemy/Enemy.h"
#include "../../game/map/MapChipField.h"
#include "../../engine/utils/LevelHotReload.h"

#include "../LevelLoader.h"

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
    // プレイヤー、敵、弾の当たり判定をまとめて処理する
    void CheckCollisions();

    // グレネード爆発範囲に入った敵を即死させる
    void CheckGrenadeExplosions();
    // 敵同士の重なりを解消する
    void ResolveEnemyOverlap();

    // 近接攻撃できる敵を探し、Eキー入力とキル演出を処理する
    void UpdateMeleeAttack();
    // マップCSVとレベルデータから床と壁のオブジェクトを作る
    void CreateMapObjects();
    // レベルデータから敵を生成する
    void SpawnEnemies();
    // レベルデータを読み直してゲーム内オブジェクトを再構築する
    void ReloadLevel(bool isManualReload);
    // レベルデータからプレイヤーのスポーン位置を更新する
    void ApplyPlayerSpawnFromLevelData(const LevelData& levelData);
private:
    std::unique_ptr<Object3d> object3d_;

    std::unique_ptr<SkyboxCommon> skyboxCommon_;
    std::unique_ptr<Skybox> skybox_;

    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::unique_ptr<ParticleSystem> particleSystem_;

    // 全敵で共有する通常アルファ合成の血しぶき用パーティクル
    std::unique_ptr<ParticleSystem> bloodParticleSystem_;

    // グレネード爆発後に長く残す煙用パーティクル
    std::unique_ptr<ParticleSystem> grenadeSmokeParticleSystem_;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

    bool initialized_ = false;

    std::unique_ptr<DebugCamera> debugCamera_;
    std::unique_ptr<FollowCamera> followCamera_;
    std::unique_ptr<Line3DCommon> line3dCommon_;
    DebugLine3D enemyVisionDebug_;

private:
    /// プレイヤー
    std::unique_ptr<Player> player_;
    Vector3 playerTranslate_ = { 2.0f, 0.5f, 0.0f };
    Vector3 playerRotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 playerScale_ = { 1.0f, 1.0f, 1.0f };

    /// 敵
    // 現在出現している敵
    std::vector<std::unique_ptr<Enemy>> enemies_;
    // 自動配置で出現させる敵の数
    uint32_t enemyCount_ = 10;
    // プレイヤー周辺に敵を自動配置するときの半径
    float enemySpawnRadius_ = 8.0f;

    // 現在近接攻撃の対象になっている敵
    Enemy* meleeTarget_ = nullptr;

    // 敵へ近接攻撃できる距離
    float meleeAttackRange_ = 7.0f;

    // 近接キル演出中か
    bool isMeleeAttacking_ = false;

    // 近接キル演出の経過フレーム
    int meleeAttackTimer_ = 0;

    // 斬撃を当てるフレーム
    int meleeAttackHitFrame_ = 6;

    // 近接キル演出で倒す敵
    Enemy* meleeVictim_ = nullptr;

    // 演出開始前のプレイヤー位置
    Vector3 meleeStartPosition_ = { 0.0f, 0.0f, 0.0f };

    // 敵へ踏み込んだときのプレイヤー位置
    Vector3 meleeStrikePosition_ = { 0.0f, 0.0f, 0.0f };


    // 近接攻撃可能な敵の頭上に表示するマーク
    std::unique_ptr<Sprite> meleeMarker_;

    // グレネード爆発で敵を即死させる半径
    float grenadeExplosionRadius_ = 4.0f;

    /// マップ
    // 読み込んだマップチップ情報
    MapChipField mapField_;
    // 1マス分の大きさ
    float tileSize_ = 2.0f;
    // レベルデータから作った床オブジェクト
    std::vector<std::unique_ptr<Object3d>> floorObjects_;
    // レベルデータから作った壁オブジェクト
    std::vector<std::unique_ptr<Object3d>> wallObjects_;

    // Blender JSONから読み込んだ床コライダー
    std::vector<LevelColliderData> floorColliders_;

    // Blender JSONから読み込んだ壁コライダー
    std::vector<LevelColliderData> wallColliders_;

    // レベルJSONの更新を監視する
    LevelHotReload levelHotReload_;
    // 監視して再読み込みするレベルJSONのパス
    std::string levelFilePath_ = "Resources/level/testScene.json";
    // リロード後にImGuiへ表示するメッセージ
    std::string reloadNoticeText_;
    // リロード通知をImGuiに表示する残りフレーム数
    uint32_t reloadNoticeFrameCount_ = 0;
};
