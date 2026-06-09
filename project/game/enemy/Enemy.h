#pragma once

#include <memory>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/math/Math.h"
#include "../collision/Collision.h"
#include "../common/WaypointMover.h"
#include "../map/MapChipField.h"
#include "../../scene/LevelLoader.h"

class Object3dCommon;

class Enemy
{
public:
    // 敵を初期化する
    void Initialize(Object3dCommon* object3dCommon, Camera* camera, const Vector3& position);

    // 敵を更新する
    void Update();

    // 敵を描画する
    void Draw();

    // 敵の座標を設定する
    void SetPosition(const Vector3& position);

    // 敵の座標を取得する
    Vector3 GetWorldPosition() const;

    // 敵の当たり判定を返す
    SphereCollider GetCollider() const;

    // 敵がダメージを受けた時の処理
    void OnHit();

    // 敵が倒されたかを返す
    bool IsDead() const { return isDead_; }

    // 追いかける対象の座標を設定する
    void SetTargetPosition(const Vector3& targetPosition);
    void SetWaypoints(const std::vector<Vector3>& waypoints);

    // 敵の押し戻し用半径を返す
    float GetBodyRadius() const { return bodyRadius_; }

    // 敵にマップ情報を渡す
    void SetMap(const MapChipField* mapField, float tileSize);

    // Blender JSON から読んだ床コライダー一覧を受け取る
    void SetFloorColliders(const std::vector<LevelColliderData>* floorColliders) {
        floorColliders_ = floorColliders;
    }
    // Blender JSON から読んだ壁コライダー一覧を受け取る
    void SetWallColliders(const std::vector<LevelColliderData>* wallColliders) {
        wallColliders_ = wallColliders;
    }

    // // デバッグカメラ確認用に描画行列だけ更新する
    void UpdateRenderOnly();
private:
    // 敵の左方向の壁当たり判定を行う
    void ResolveLeftCollisionWithMap(Vector3& pos);

    // 敵の右方向の壁当たり判定を行う
    void ResolveRightCollisionWithMap(Vector3& pos);

    // 敵の上方向の壁当たり判定を行う
    void ResolveTopCollisionWithMap(Vector3& pos);

    // 敵の下方向の壁当たり判定を行う
    void ResolveBottomCollisionWithMap(Vector3& pos);

    // 床コライダーから現在位置の地面高さを合わせる
    void ResolveGroundHeight(Vector3& pos);

    // 壁コライダーとの横移動衝突を解決する
    void ResolveWallCollision(Vector3& pos);
private:
    // 敵の3Dオブジェクト
    WaypointMover waypointMover_;

    // 敵の座標
    Vector3 position_ = { 0.0f, 0.5f, 0.0f };

    // 敵の回転
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    // 敵の大きさ
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    // 敵の当たり判定半径
    float colliderRadius_ = 1.0f;

    // 倒された敵を管理する
    bool isDead_ = false;

    // 追いかける対象の座標
    Vector3 targetPosition_ = { 0.0f, 0.0f, 0.0f };

    // 敵の移動速度
    float moveSpeed_ = 0.1f;

    // 敵同士が埋まらないようにするための半径
    float bodyRadius_ = 1.0f;

    // 前フレームの座標を保持する
    Vector3 prevPosition_ = { 0.0f, 0.5f, 0.0f };

    // マップデータへの参照
    const MapChipField* mapField_ = nullptr;

    // 1マスの大きさ
    float tileSize_ = 2.0f;

    // Blender JSON から読んだ床コライダー一覧
    const std::vector<LevelColliderData>* floorColliders_ = nullptr;

    // Blender JSON から読んだ壁コライダー一覧
    const std::vector<LevelColliderData>* wallColliders_ = nullptr;

    // プレイヤーを見つける距離
    float detectRange_ = 6.0f;

    // いったん見つけた後に追いかけ続ける距離
    float chaseKeepRange_ = 20.0f;

    // 追尾中かどうか
    bool isChasing_ = false;
};
