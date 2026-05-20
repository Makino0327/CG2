#pragma once

#include <memory>
#include <vector>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/input/Input.h"
#include "../map/MapChipField.h"
#include "../camera/Camera.h"
#include "../collision/Collision.h"
#include "PlayerBullet.h"

class Player
{
public:
    void Initialize(Object3dCommon* object3dCommon, Input* input);

    void Update(Camera* camera);

    void Draw();

    // マップ情報とタイルサイズを設定する
    void SetMap(MapChipField* mapField, float tileSize) {
        mapField_ = mapField;
        tileSize_ = tileSize;
    }

    // プレイヤーの座標を返す
    Vector3 GetWorldPosition() const;

    // プレイヤーの当たり判定を返す
    SphereCollider GetCollider() const;

    // プレイヤーが何かに当たった時の処理
    void OnHit();

    // プレイヤーが持つ弾一覧を参照できるようにする
    const std::vector<std::unique_ptr<PlayerBullet>>& GetBullets() const { return bullets_; }

    // プレイヤーの現在HPを返す
    int GetHp() const { return hp_; }

    // プレイヤーが消えたかを返す
    bool IsDead() const { return isDead_; }

    // プレイヤーを復活させる
    void Respawn();

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

    // マウスの方向へプレイヤーを向ける
    void RotateToMouse(Camera* camera);

    // モデルの正面方向補正に使う角度
    float frontAngleOffset_ = 0.0f;

private:
    std::unique_ptr<Object3d> object_;
    Input* input_ = nullptr;

    // 3Dオブジェクト共通設定を保持する
    Object3dCommon* object3dCommon_ = nullptr;

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
    float colliderRadius_ = 0.8f;

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

    // 弾の速度
    float bulletSpeed_ = 0.5f;

    // 弾の発射位置の高さ
    float bulletSpawnHeight_ = 0.5f;
};
