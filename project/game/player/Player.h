#pragma once
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/input/Input.h"
#include "../map/MapChipField.h"
#include <memory>

#include "PlayerBullet.h"
#include "../camera/Camera.h"
#include <vector>

class Player
{
public:
    void Initialize(Object3dCommon* object3dCommon, Input* input);

    void Update(Camera* camera);

    void Draw();

    // 追加：マップとタイルサイズを渡す
    void SetMap(MapChipField* mapField, float tileSize) {
        mapField_ = mapField;
        tileSize_ = tileSize;
    }
    // プレイヤーの位置を返す
    Vector3 GetWorldPosition() const;

private:
    // 下方向の当たり判定だけを行う新しい関数
    void ResolveBottomCollisionWithMap(Vector3& pos);

    void ResolveLeftCollisionWithMap(Vector3& pos);

    void ResolveTopCollisionWithMap(Vector3& pos);

    void ResolveRightCollisionWithMap(Vector3& pos);

    // 弾を発射する
    void FireBullet(Camera* camera);

    // 弾を更新する
    void UpdateBullets();

    // マウスの方向へプレイヤーを向ける
    void RotateToMouse(Camera* camera);

    // モデルの正面方向を補正する角度
    float frontAngleOffset_ = 0.0f;

private:
    std::unique_ptr<Object3d> object_;
    Input* input_ = nullptr;
    // 3D描画の共通設定
    Object3dCommon* object3dCommon_ = nullptr;

    // 物理系
    float moveSpeed_ = 0.2f;
    float velocityY_ = 0.0f;
    float jumpPower_ = 0.6f;
    float gravity_ = -0.03f;
    bool  onGround_ = false;

    // マップ情報
    MapChipField* mapField_ = nullptr;
    float tileSize_ = 2.0f; // main の kTileSize と同じにする

    Vector3 prevPos_{};

    // プレイヤーの初期位置
    Vector3 translate_ = { 0.0f, 0.5f, 0.0f };

    // プレイヤーの初期回転
    Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

    // プレイヤーの大きさ
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    // プレイヤー弾
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;

    // プレイヤー弾の速さ
    float bulletSpeed_ = 0.5f;

    // 弾を出す高さ
    float bulletSpawnHeight_ = 0.5f;


};
