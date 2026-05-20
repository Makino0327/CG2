#pragma once
#include <memory>
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/math/Math.h"
#include "../collision/Collision.h"
#include "../map/MapChipField.h"
class Camera;
class Input;

class PlayerBullet
{
public:
    void Initialize(
        Object3dCommon* object3dCommon,
        const Vector3& position,
        const Vector3& velocity,
        const MapChipField* mapField,
        float tileSize);

    void Update();
    void Draw();

    // 弾の当たり判定を返す
    SphereCollider GetCollider() const;

    // 弾が何かに当たった時に消す
    void OnHit();

    bool IsDead() const { return isDead_; }

    static Vector3 CalcDirectionToMouseGround(
        const Vector3& startPosition,
        Camera* camera,
        Input* input);

private:
    static Vector3 GetMousePositionOnGround(Camera* camera, Input* input);

private:
    std::unique_ptr<Object3d> object_;

    // 弾の位置
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

    // 弾の速度
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };

    // 弾の大きさ
    Vector3 scale_ = { 0.5f, 0.5f, 0.5f };

    // 弾の当たり判定半径
    float colliderRadius_ = 0.4f;

    // 生存フレーム
    int lifeTime_ = 120;

    // 消すかどうか
    bool isDead_ = false;

    // 弾が参照するマップ情報
    const MapChipField* mapField_ = nullptr;

    // 1マスの大きさ
    float tileSize_ = 2.0f;
};
