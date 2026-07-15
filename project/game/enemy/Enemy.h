#pragma once

#include <array>
#include <memory>
#include <vector>

#include "../../engine/3d/obj3d/DebugLine3D.h"
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/math/Math.h"
#include "../collision/Collision.h"
#include "../common/WaypointMover.h"
#include "../map/MapChipField.h"
#include "../../scene/LevelLoader.h"

class Object3dCommon;
class ParticleSystem;

class Enemy
{
public:
    // 敵を初期化する
    void Initialize(Object3dCommon* object3dCommon, Camera* camera, const Vector3& position);

    // 敵を更新する
    void Update();

    // 敵を描画する
    void Draw();

    // 位置を設定する
    void SetPosition(const Vector3& position);

    // 現在位置を取得する
    Vector3 GetWorldPosition() const;

    // 当たり判定を取得する
    SphereCollider GetCollider() const;

    // 被弾時の処理を行う
    void OnHit(const Vector3& hitPosition, const Vector3& hitDirection);

    // グレネードの爆発範囲へ入った敵を即死させる
    void OnExplosionHit();

    // シーンが所有する血しぶき用パーティクルを設定する
    void SetBloodParticleSystem(ParticleSystem* particleSystem) {
        bloodParticleSystem_ = particleSystem;
    }

    // 現在HPを返す
    int GetHp() const { return hp_; }

    // 撃破済みかを返す
    bool IsDead() const { return isDead_; }

    // 死亡演出が終わり、敵を配列から削除してよいか返す
    bool IsReadyToRemove() const { return isReadyToRemove_; }

    // 銃声などの音を聞いたときに音のした位置へ振り向かせる
    void OnHearSound(const Vector3& soundPosition);

    // 追跡対象の位置を設定する
    void SetTargetPosition(const Vector3& targetPosition);
    void SetWaypoints(const std::vector<Vector3>& waypoints);

    // 重なり解消用の半径を返す
    float GetBodyRadius() const { return bodyRadius_; }

    // マップ情報を設定する
    void SetMap(const MapChipField* mapField, float tileSize);
    bool IsTargetInSight() const { return isTargetInSight_; }

    // 一度でもプレイヤーを発見したかを返す
    bool HasDetectedPlayer() const { return hasDetectedPlayer_; }

    // 近接攻撃によって敵を即死させる
    void OnMeleeHit();

    // 通常近接攻撃で敵に1ダメージを与える
    void OnMeleeDamage(const Vector3& hitPosition, const Vector3& hitDirection);

    void AppendVisionDebugLines(DebugLine3D& debugLine) const;

    // 床コライダーを設定する
    void SetFloorColliders(const std::vector<LevelColliderData>* floorColliders) {
        floorColliders_ = floorColliders;
    }
    // 壁コライダーを設定する
    void SetWallColliders(const std::vector<LevelColliderData>* wallColliders) {
        wallColliders_ = wallColliders;
    }

    // 敵AI用のNavMeshを設定する
    void SetNavMesh(const LevelNavMeshData* navMesh);

    // デバッグカメラ用に描画だけ更新する
    void UpdateRenderOnly();
private:
    // 左方向のマップ衝突を解決する
    void ResolveLeftCollisionWithMap(Vector3& pos);

    // 右方向のマップ衝突を解決する
    void ResolveRightCollisionWithMap(Vector3& pos);

    // 前方向のマップ衝突を解決する
    void ResolveTopCollisionWithMap(Vector3& pos);

    // 後方向のマップ衝突を解決する
    void ResolveBottomCollisionWithMap(Vector3& pos);

    // 床コライダーから接地高さを求める
    void ResolveGroundHeight(Vector3& pos);

    // 壁コライダーとの衝突を解決する
    void ResolveWallCollision(Vector3& pos);
    bool CheckTargetInSight() const;

    // 指定位置に移動したとき壁と重なるか調べる
    bool IsPositionBlockedByWall(const Vector3& pos) const;

    // 始点から終点までの直線が壁に遮られるかを調べる
    bool IsSegmentBlockedByWall(
        const Vector3& start,
        const Vector3& end,
        const LevelColliderData** hitWall,
        const LevelColliderData* ignoredWall = nullptr) const;

    // 壁の角を一時目標にして追跡方向を決める
    Vector3 CalculateChaseDirection(const Vector3& chaseTarget) const;

    // NavMesh上の経路から追跡方向を決める
    Vector3 CalculateNavMeshChaseDirection(const Vector3& chaseTarget);

    // NavMeshの三角形同士のつながりを作る
    void BuildNavMeshLinks();

    // 指定位置が乗っているNavMesh三角形を探す
    int FindNavMeshTriangle(const Vector3& position) const;

    // NavMesh上で開始三角形から目標三角形までの経路を探す
    bool FindNavMeshPath(int startTriangle, int goalTriangle, std::vector<int>& outPath) const;

    // NavMesh三角形の中心位置を返す
    Vector3 GetNavMeshTriangleCenter(int triangleIndex) const;

    // 被弾方向へ血しぶきを生成する
    void EmitBloodSplatter(const Vector3& hitPosition, const Vector3& hitDirection);

    // 死亡時の大きな血しぶきと破片を開始する
    void StartDeathEffect();

    // 死亡時の大きな血しぶきを敵の中心から発生させる
    void EmitDeathBurst();

    // 飛んでいる敵の破片を更新する
    void UpdateDeathFragments();

    // 指定したXZ座標にある床の高さを取得する
    bool GetGroundHeight(float x, float z, float& groundY) const;
private:
    // 敵の移動と描画を管理する
    WaypointMover waypointMover_;

    // 現在位置
    Vector3 position_ = { 0.0f, 0.5f, 0.0f };

    // 回転
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    // 拡大率
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    // 球コライダーの半径
    float colliderRadius_ = 1.0f;

    // 撃破フラグ
    bool isDead_ = false;

    // 敵の現在HP
    int hp_ = 3;

    // 敵の最大HP
    int maxHp_ = 3;

    // シーンで共有する血しぶき用パーティクル
    ParticleSystem* bloodParticleSystem_ = nullptr;

    // 追跡対象の位置
    Vector3 targetPosition_ = { 0.0f, 0.0f, 0.0f };

    // 移動速度
    float moveSpeed_ = 0.1f;

    // 敵同士の重なり解消に使う半径
    float bodyRadius_ = 1.0f;

    // 前フレームの位置
    Vector3 prevPosition_ = { 0.0f, 0.5f, 0.0f };

    // マップデータへの参照
    const MapChipField* mapField_ = nullptr;

    // 1マスの大きさ
    float tileSize_ = 2.0f;

    // 床コライダー一覧
    const std::vector<LevelColliderData>* floorColliders_ = nullptr;

    // 壁コライダー一覧
    const std::vector<LevelColliderData>* wallColliders_ = nullptr;

    // 敵AIが使うNavMesh
    const LevelNavMeshData* navMesh_ = nullptr;

    // NavMesh三角形ごとの隣接三角形一覧
    std::vector<std::vector<int>> navMeshNeighbors_;

    // 現在使っているNavMesh経路
    std::vector<int> navMeshPath_;

    // NavMesh経路を再計算するまでの残りフレーム
    int navMeshPathRefreshTimer_ = 0;

    // NavMesh追跡中の進行方向をなめらかにするために保存する
    Vector3 navMeshSmoothedDirection_ = { 0.0f, 0.0f, 0.0f };

    // 視認できる距離
    float detectRange_ = 12.0f;
    float sightHalfAngleRad_ = 0.785398163f;
    // 上下の半角は45度にして、tan()の暴走を防ぐ
    float sightVerticalHalfAngleRad_ = 0.785398163f;
    float sightHeight_ = 1.0f;

    // 追跡を維持する最大距離
    float chaseKeepRange_ = 20.0f;

    // 追跡中かどうか
    bool isChasing_ = false;
    bool isTargetInSight_ = false;

    // 一度でもプレイヤーを発見したか
    // 追跡をやめて巡回へ戻っても解除しない
    bool hasDetectedPlayer_ = false;

    // 前フレームで追跡していたか
    bool wasChasing_ = false;

    // 追跡終了後にNavMeshで巡回地点へ戻っているか
    bool isReturningToPatrol_ = false;

    // 視界から外れてもすぐ見失わないための猶予フレーム
    int lostSightGraceTimer_ = 0;

    // 見失ったあと、その場で周囲を見る残りフレーム
    int lostSightLookTimer_ = 0;

    // 見失った瞬間の向き
    float lostSightLookStartYaw_ = 0.0f;

    // 聞こえた音の位置
    Vector3 heardSoundPosition_ = { 0.0f, 0.0f, 0.0f };

    // 音の方向を向いて警戒している残りフレーム(0なら通常状態)
    int hearingTimer_ = 0;

    // 音の方向へ振り向くときの旋回速度(ラジアン/フレーム)
    float hearingTurnSpeed_ = 0.12f;

    // 敵OBJを再利用した破片1個分の情報
    struct DeathFragment {
        // 破片として描画する3Dオブジェクト
        std::unique_ptr<Object3d> object;

        // 現在位置
        Vector3 position = { 0.0f, 0.0f, 0.0f };

        // 現在の回転
        Vector3 rotation = { 0.0f, 0.0f, 0.0f };

        // 移動速度
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };

        // 回転速度
        Vector3 angularVelocity = { 0.0f, 0.0f, 0.0f };

        // 破片の大きさ
        Vector3 scale = { 0.25f, 0.25f, 0.25f };

        // 床に落ちて停止したか
        bool isLanded = false;
    };

    // 死亡時に飛ばす破片
    std::array<DeathFragment, 7> deathFragments_;

    // 破片生成に使用する描画共通情報
    Object3dCommon* object3dCommon_ = nullptr;

    // 破片へ設定するカメラ
    Camera* camera_ = nullptr;

    // 死亡演出の経過時間
    float deathEffectTimer_ = 0.0f;

    // 死亡演出終了後、削除可能になったか
    bool isReadyToRemove_ = false;
};
