#pragma once
#include <memory>
#include <vector>
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/math/Math.h"
#include "../collision/Collision.h"
#include "../../scene/LevelLoader.h"
class Camera;
class Input;
class ParticleSystem;

class PlayerBullet
{
public:
    void Initialize(
        Object3dCommon* object3dCommon,
        const Vector3& position,
        const Vector3& velocity,
        const std::vector<LevelColliderData>* wallColliders,
        ParticleSystem* particleSystem);

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

    // 1フレームの移動区間へ多層の発光粒子を並べる
    void EmitTrail(const Vector3& start, const Vector3& end);

    // 弾の周囲へ短時間で消える火花を生成する
    void EmitSparks();

private:
    std::unique_ptr<Object3d> object_;

    // 弾が通過した位置へ軌跡を生成する共有パーティクルシステム
    ParticleSystem* particleSystem_ = nullptr;

    // 火花の生成間隔を管理するフレームカウンター
    int sparkFrame_ = 0;

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
    const std::vector<LevelColliderData>* wallColliders_ = nullptr;

    // 1マスの大きさ
};
