#pragma once

#include <memory>

#include "../../engine/math/Math.h"
#include "../collision/Collision.h"

class Camera;
class Object3d;
class Object3dCommon;

class Boss
{
public:
    // Object3dの解放をcpp側で行えるようにする
    ~Boss();

    // ボスを初期化して、Blenderで置いた位置と大きさを反映する
    void Initialize(
        Object3dCommon* object3dCommon,
        Camera* camera,
        const Vector3& position,
        const Vector3& rotation,
        const Vector3& scale);

    // ボスの描画用Transformを更新する
    void Update();

    // ボスモデルを描画する
    void Draw();

    // ボスの当たり判定を返す
    SphereCollider GetCollider() const;

    // 弾が当たった時にHPを減らす
    void OnHit();

    // 現在HPを返す
    int GetHp() const { return hp_; }

    // 最大HPを返す
    int GetMaxHp() const { return maxHp_; }

    // 撃破済みかを返す
    bool IsDead() const { return isDead_; }

    // 撃破演出が終わり、削除してよいかを返す
    bool IsReadyToRemove() const { return isReadyToRemove_; }

private:
    // ボスの3Dモデル本体
    std::unique_ptr<Object3d> object_;

    // 現在位置
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

    // 回転
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    // Blenderから読み込んだ元の拡大率
    Vector3 baseScale_ = { 1.0f, 1.0f, 1.0f };

    // ボスの現在HP
    int hp_ = 12;

    // ボスの最大HP
    int maxHp_ = 12;

    // 球コライダーの半径
    float colliderRadius_ = 3.0f;

    // 撃破済みか
    bool isDead_ = false;

    // 撃破演出が終わったか
    bool isReadyToRemove_ = false;

    // 撃破演出の経過フレーム
    int deathTimer_ = 0;

    // 撃破演出の長さ
    int deathDuration_ = 90;
};
