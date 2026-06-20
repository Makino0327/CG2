#pragma once

#include <memory>
#include <vector>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/math/Math.h"
#include "../../scene/LevelLoader.h"

class Camera;
class Object3dCommon;

class PlayerGrenade
{
public:
    // 投擲開始位置と方向を受け取り、グレネードを初期化する
    void Initialize(
        Object3dCommon* object3dCommon,
        Camera* camera,
        const Vector3& position,
        const Vector3& throwDirection,
        const std::vector<LevelColliderData>* floorColliders,
        const std::vector<LevelColliderData>* wallColliders);

    // 放物運動、床と壁の反射、爆発タイマーを更新する
    void Update();

    // 爆発前のグレネードを描画する
    void Draw();

    // 爆発処理が終わり、Playerの配列から削除してよいか返す
    bool IsDead() const { return isDead_; }

    // 爆発が始まったか返す
    bool IsExploded() const { return isExploded_; }

    // 爆発位置を一度だけPlayerへ渡す
    bool ConsumeExplosion(Vector3& explosionPosition);

    // 現在のワールド座標を返す
    const Vector3& GetWorldPosition() const { return position_; }

private:
    // 現在位置にある床の上面を取得する
    bool GetGroundHeight(float x, float z, float& groundY) const;

    // 壁へ入り込んだ分を戻し、速度を反射させる
    void ResolveWallCollision(const Vector3& previousPosition);

    // 1秒後に呼ばれる爆発処理をまとめる
    void Explode();

private:
    // グレネードの3Dモデル
    std::unique_ptr<Object3d> object_;

    // 現在位置
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

    // 移動速度
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };

    // 現在の回転
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    // 投擲中の回転速度
    Vector3 angularVelocity_ = { 0.11f, 0.16f, 0.08f };

    // Blender JSONから読み込んだ床コライダー一覧
    const std::vector<LevelColliderData>* floorColliders_ = nullptr;

    // Blender JSONから読み込んだ壁コライダー一覧
    const std::vector<LevelColliderData>* wallColliders_ = nullptr;

    // 球として扱うグレネードの半径
    float colliderRadius_ = 0.24f;

    // 最初に床へ接触してからの経過フレーム
    int fuseTimer_ = 0;

    // 床への接触後、60FPSで約1秒後に爆発させる
    int fuseDuration_ = 60;

    // 一度でも床へ接触し、爆発待ちに入ったか
    bool hasTouchedGround_ = false;

    // 床でほぼ停止したか
    bool isLanded_ = false;

    // 爆発処理を開始したか
    bool isExploded_ = false;

    // 爆発通知をPlayerへ渡し終えたか
    bool isExplosionConsumed_ = false;

    // Playerの配列から削除してよいか
    bool isDead_ = false;
};