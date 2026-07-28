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

#include "../../game/minimap/Minimap.h"

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
    // 弾の発射位置に画面歪みを出す
    void StartBulletShockwaves();
    // 敵同士の重なりを解消する
    void ResolveEnemyOverlap();

    // ナイフモード中に近接攻撃できる敵を探し、左クリック入力と攻撃演出を処理する
    void UpdateMeleeAttack();
    // ナイフで切ったような斬撃エフェクトを出す
    void EmitMeleeSlashEffect(const Vector3& center, const Vector3& direction);
    // マップCSVとレベルデータから床と壁のオブジェクトを作る
    void CreateMapObjects();
    // レベルデータから敵を生成する
    void SpawnEnemies();
    // レベルデータを読み直してゲーム内オブジェクトを再構築する
    void ReloadLevel(bool isManualReload);
    // レベルデータからプレイヤーのスポーン位置を更新する
    void ApplyPlayerSpawnFromLevelData(const LevelData& levelData);
    // 左下の残弾UI用Spriteを現在の弾数に合わせて更新する
    void UpdateAmmoUiSprites();
    // 左下の残弾UI用Spriteを描画する
    void DrawAmmoUiSprites();
    // 発射した弾が前へ飛び出して消える演出を開始する
    void SpawnAmmoFireEffect(const Vector2& center, const Vector2& size);
    // 発射演出の経過を進めてSpriteへ反映する
    void UpdateAmmoFireEffects();
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

    // 普通の近接攻撃が届く距離
    float normalMeleeAttackRange_ = 3.6f;

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

    // 左下に表示する残弾UI用の黄色い四角Sprite
    std::vector<std::unique_ptr<Sprite>> ammoSprites_;

    // 残弾UIを見えやすくする黒い半透明の背景パネル用Sprite
    std::unique_ptr<Sprite> ammoBackgroundSprite_;

    // 発射した弾が前へ飛び出して消える演出の状態
    struct AmmoFireEffect {
        Vector2 position{};   // 弾の中心位置
        Vector2 size{};       // 弾のサイズ
        float timer = 0.0f;   // 経過フレーム
        bool active = false;  // 演出中かどうか
    };
    // 発射演出の状態(連射に備えて複数持つ)
    std::vector<AmmoFireEffect> ammoFireEffects_;
    // 発射演出用のSprite(ammoFireEffects_と同じ数)
    std::vector<std::unique_ptr<Sprite>> ammoFireEffectSprites_;

    // 弾1発ごとの移動・フェードのアニメーション状態
    struct AmmoBulletAnim {
        Vector2 position{};   // 現在の描画位置
        float alpha = 0.0f;   // 現在の透明度
        float delay = 0.0f;   // リロードで順番に入ってくるまでの待ちフレーム
    };
    // 弾ごとのアニメーション状態(ammoSprites_と同じ数)
    std::vector<AmmoBulletAnim> ammoBulletAnims_;

    // 発射とリロードを検出するための前フレームの残弾数
    int ammoUiPrevAmmo_ = -1;
    // 武器切り替えを検出するための前フレームの最大弾数
    int ammoUiPrevMaxAmmo_ = -1;
    // 武器切り替えを検出するための前フレームの攻撃モード
    Player::AttackMode ammoUiPrevMode_ = Player::AttackMode::Knife;

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

    // 敵AI用のNavMeshデータ
    LevelNavMeshData navMeshData_;

    // レベルJSONの更新を監視する
    LevelHotReload levelHotReload_;
    // 監視して再読み込みするレベルJSONのパス
    std::string levelFilePath_ = "Resources/level/testScene.json";
    // リロード後にImGuiへ表示するメッセージ
    std::string reloadNoticeText_;
    // リロード通知をImGuiに表示する残りフレーム数
    uint32_t reloadNoticeFrameCount_ = 0;

    // ミニマップの読み込み、更新、描画を管理する
    std::unique_ptr<Minimap> minimap_;

    // 銃を構えた時のビネットをゆっくり変化させるための現在値
    float aimingVignetteIntensity_ = 0.0f;
    // ビネットが濃くなる速さ
    float aimingVignetteFadeInSpeed_ = 0.045f;
    // ビネットが薄くなる速さ
    float aimingVignetteFadeOutSpeed_ = 0.060f;

    // 発射や爆発の後に輝度アウトラインを残すフレーム数
    int luminanceOutlineTimer_ = 0;
    // 被弾や爆発の後にボックスフィルタを残すフレーム数
    int boxFilterTimer_ = 0;
    // 被弾、爆発、ショットガン発射後にRGBずれを残すフレーム数
    int chromaticAberrationTimer_ = 0;
    // 発砲、爆発、ステルス近接キル後にBloomを残すフレーム数
    int bloomTimer_ = 0;
    // 前フレームのプレイヤーHPを保存し、被弾した瞬間を検知する
    int previousPlayerHp_ = 3;
    // 死亡時のディゾルブを何度も最初から再生しないためのフラグ
    bool deathDissolveStarted_ = false;
};
