#pragma once

#include <memory>
#include <vector>

#include "../../engine/math/Math.h"
#include "../collision/Collision.h"

class Camera;
class Object3d;
class Object3dCommon;
struct LevelColliderData;

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

    // プレイヤーを追跡し、予備動作・攻撃・硬直・撃破演出を更新する
    void Update(const Vector3& targetPosition, bool canAttack);

    // デバッグ中は行動時間を進めず、描画用の行列だけ更新する
    void UpdateRenderOnly();

    // ステージの壁を参照して、追跡中の壁抜けを防ぐ
    void SetWallColliders(const std::vector<LevelColliderData>* walls) { wallColliders_ = walls; }

    // 予告円と着地エフェクトに使う攻撃範囲を返す
    bool IsWindingUp() const { return !isDead_ && action_ == Action::Windup; }
    bool IsImpactFrame() const { return !isDead_ && impactThisFrame_; }
    const Vector3& GetAttackCenter() const { return attackCenter_; }
    float GetAttackRadius() const { return attackRadius_; }

    // 着地した瞬間だけ攻撃範囲内のプレイヤーに命中する
    bool IsAttackHit(const SphereCollider& target) const;

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
    // 巨大ゾンビは追跡、溜め、攻撃後の隙を順に繰り返す
    enum class Action { Chase, Windup, Recovery };

    // 壁の手前で移動を止め、壁に沿って進めるようにする
    void MoveTowards(const Vector3& targetPosition);

    // 攻撃を開始した時点で狙う場所を固定し、回避できるようにする
    void BeginAttack(const Vector3& targetPosition);

    Action action_ = Action::Chase; // 現在の行動
    int actionTimer_ = 0; // 現在の行動の経過フレーム
    int attackCount_ = 0; // 3回に1回、周囲への踏みつけを行う
    bool stompAttack_ = false; // 周囲への踏みつけか
    bool impactThisFrame_ = false; // 着地した1フレームだけ有効
    Vector3 attackCenter_{}; // 予告と命中判定で共有する中心
    float attackRadius_ = 0.0f; // 予告と命中判定で共有する半径
    float walkPhase_ = 0.0f; // 重い歩行の揺れに使う位相
    float moveSpeed_ = 0.07f; // プレイヤーより遅い1フレームの移動量
    const std::vector<LevelColliderData>* wallColliders_ = nullptr; // シーン所有の壁

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
