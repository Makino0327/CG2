#pragma once

#include <memory>
#include <vector>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/input/Input.h"
#include "../map/MapChipField.h"
#include "../camera/Camera.h"
#include "../collision/Collision.h"
#include "PlayerBullet.h"
#include "PlayerGrenade.h"

#include "../../scene/LevelLoader.h"

class ParticleSystem;

class Player
{
public:
    void Initialize(
        Object3dCommon* object3dCommon,
        Input* input,
        ParticleSystem* particleSystem);

    void Update(Camera* camera);

    void Draw();

    // マップ情報とタイルサイズを設定する
    void SetMap(MapChipField* mapField, float tileSize) {
        mapField_ = mapField;
        tileSize_ = tileSize;
    }

    // Blender JSON から読んだ床コライダー一覧を受け取る
    void SetFloorColliders(const std::vector<LevelColliderData>* floorColliders) {
        floorColliders_ = floorColliders;
    }

    // プレイヤーの座標を返す
    Vector3 GetWorldPosition() const;

    // プレイヤーの当たり判定を返す
    SphereCollider GetCollider() const;

    // プレイヤーが何かに当たった時の処理
    void OnHit();

    // プレイヤーが持つ弾一覧を参照できるようにする
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return bullets_; }

    // グレネード爆発によるカメラシェイク要求を一度だけ返す
    bool ConsumeGrenadeShakeRequest();

    // 未処理のグレネード爆発位置をSceneへまとめて渡す
    std::vector<Vector3> ConsumeGrenadeExplosions();

    // Sceneが所有する煙用の通常アルファパーティクルを設定する
    void SetGrenadeSmokeParticleSystem(ParticleSystem* particleSystem) {
        grenadeSmokeParticleSystem_ = particleSystem;
    }

    // プレイヤーの現在HPを返す
    int GetHp() const { return hp_; }

    // プレイヤーが消えたかを返す
    bool IsDead() const { return isDead_; }

    // プレイヤーを復活させる
    void Respawn();

    // Blender JSON から読んだ壁コライダー一覧を受け取る
    void SetWallColliders(const std::vector<LevelColliderData>* wallColliders) {
        wallColliders_ = wallColliders;
    }

    // // デバッグカメラ確認用に描画行列だけ更新する
    void UpdateRenderOnly();

    // Blender から読んだプレイヤー開始位置を設定する
    void SetSpawnPosition(const Vector3& spawnPosition) {
        translate_ = spawnPosition;
    }

private:
    // 下方向のマップ当たり判定を処理する
    void ResolveBottomCollisionWithMap(Vector3& pos);

    // 左方向のマップ当たり判定を処理する
    void ResolveLeftCollisionWithMap(Vector3& pos);

    // 上方向のマップ当たり判定を処理する
    void ResolveTopCollisionWithMap(Vector3& pos);

    // 右方向のマップ当たり判定を処理する
    void ResolveRightCollisionWithMap(Vector3& pos);

    // 弾を発射する
    void FireBullet(Camera* camera);

    // 弾を更新する
    void UpdateBullets();

    // プレイヤーの向いている方向へグレネードを投げる
    void ThrowGrenade(Camera* camera);

    // 投げたグレネードを更新して、爆発後に削除する
    void UpdateGrenades();

    // 指定位置へ爆発パーティクルを発生させる
    void EmitGrenadeExplosion(const Vector3& explosionPosition);

    // マウスの方向へプレイヤーを向ける
    void RotateToMouse(Camera* camera);

    // 床コライダーから現在位置の地面高さを合わせる
    void ResolveGroundHeight(Vector3& pos);

    // 壁コライダーとの横移動衝突を解決する
    void ResolveWallCollision(Vector3& pos);

    

    // モデルの正面方向補正に使う角度
    float frontAngleOffset_ = 0.0f;

private:
    std::unique_ptr<Object3d> object_;
    Input* input_ = nullptr;

    // 3Dオブジェクト共通設定を保持する
    Object3dCommon* object3dCommon_ = nullptr;

    // プレイヤー弾が共有する軌跡用パーティクルシステム
    ParticleSystem* particleSystem_ = nullptr;

    // グレネード爆発後に残す煙用パーティクルシステム
    ParticleSystem* grenadeSmokeParticleSystem_ = nullptr;

    // 移動関係のパラメータ
    float moveSpeed_ = 0.2f;
    float velocityY_ = 0.0f;
    float jumpPower_ = 0.6f;
    float gravity_ = -0.03f;
    bool onGround_ = false;

    // マップ情報
    MapChipField* mapField_ = nullptr;
    float tileSize_ = 2.0f;

    Vector3 prevPos_{};

    // プレイヤーの座標
    Vector3 translate_ = { 2.0f, 0.5f, 2.0f };

    // プレイヤーの回転
    Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

    // プレイヤーの大きさ
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    // プレイヤーの当たり判定半径
    float colliderRadius_ = 1.0f;

    // 被弾状態を保持する
    bool isHit_ = false;

    // プレイヤーの現在HP
    int hp_ = 3;

    // プレイヤーの最大HP
    int maxHp_ = 3;

    // 無敵時間の残りフレーム
    int invincibleTimer_ = 0;

    // 無敵時間の長さ
    int invincibleDuration_ = 60;

    // 点滅の切り替え間隔
    int blinkInterval_ = 5;

    // 消えたかどうか
    bool isDead_ = false;

    // プレイヤーが撃った弾
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;

    // プレイヤーが投げたグレネード
    std::vector<std::unique_ptr<PlayerGrenade>> grenades_;

    // GamePlaySceneへまだ渡していないカメラシェイク要求
    bool grenadeShakeRequested_ = false;

    // 敵との爆発判定をまだ処理していない爆発位置
    std::vector<Vector3> pendingGrenadeExplosions_;

    // 弾の速度
    float bulletSpeed_ = 1.4f;

    // 弾の発射位置の高さ
    float bulletSpawnHeight_ = 0.7f;

    // プレイヤー中心から射出口までの前方向距離
    float bulletMuzzleDistance_ = 1.6f;

    // グレネードの投擲開始位置の高さ
    float grenadeSpawnHeight_ = 0.8f;

    // プレイヤー中心からグレネードを出す前方向距離
    float grenadeMuzzleDistance_ = 0.8f;

    // Blender JSON から読んだ床コライダー一覧
    const std::vector<LevelColliderData>* floorColliders_ = nullptr;

    // Blender JSON から読んだ壁コライダー一覧
    const std::vector<LevelColliderData>* wallColliders_ = nullptr;
};
