#pragma once

#include <memory>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/math/Math.h"
#include "../collision/Collision.h"

class Object3dCommon;

class Enemy
{
public:
    // 敵を初期化する
    void Initialize(Object3dCommon* object3dCommon, const Vector3& position);

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

    // 敵の押し戻し用半径を返す
    float GetBodyRadius() const { return bodyRadius_; }

private:
    // 敵の3Dオブジェクト
    std::unique_ptr<Object3d> object_;

    // 敵の座標
    Vector3 position_ = { 0.0f, 0.5f, 0.0f };

    // 敵の回転
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    // 敵の大きさ
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    // 敵の当たり判定半径
    float colliderRadius_ = 0.8f;

    // 倒された敵を管理する
    bool isDead_ = false;

    // 追いかける対象の座標
    Vector3 targetPosition_ = { 0.0f, 0.0f, 0.0f };

    // 敵の移動速度
    float moveSpeed_ = 0.05f;

    // 敵同士が埋まらないようにするための半径
    float bodyRadius_ = 1.0f;

};
