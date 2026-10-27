#pragma once
#include <memory>
#include <vector>
#include <optional>
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
    // 狙い位置の選択と実際の命中判定で、同じ弾の半径を使う。
    static constexpr float kColliderRadius = 0.4f;
    void Initialize(
        Object3dCommon* object3dCommon,
        const Vector3& position,
        const Vector3& velocity,
        const std::vector<LevelColliderData>* wallColliders,
        ParticleSystem* particleSystem,
        std::optional<float> flightHeight = std::nullopt);

    // 敵の判定がある場合は、命中先が決まるまで軌跡と壁の命中演出を待つ。
    void Update(bool deferEffects = false);
    void FinishUpdate();
    void Draw();

    // 弾の当たり判定を返す
    SphereCollider GetCollider() const;

    // 弾が何かに当たった時に消す
    void OnHit(float hitTime = 1.0f);

    // このフレームの移動区間全体で敵への命中を判定する。
    bool CheckHit(const SphereCollider& target, float& hitTime) const;
    // 通常敵の箱にも、同じ移動区間で当たり判定を行う。
    bool CheckHit(const BoxCollider& target, float& hitTime) const;
    // 敵の移動前の位置も使い、追いかけながら撃つ場合の接触を見逃さない。
    bool CheckHit(const BoxCollider& target, const Vector3& previousCenter, float& hitTime) const;

    bool IsDead() const { return isDead_; }

    // 命中エフェクトへ弾の進行方向を渡すため速度を返す
    const Vector3& GetVelocity() const { return velocity_; }

    // 画面上のマウスから、カメラの手前と奥を結ぶ線を取得する。
    static bool GetMouseRay(Camera* camera, Input* input, Vector3& start, Vector3& end);

    // プレイヤーの向きと近距離の射撃判定で、同じ地面上のマウス位置を共有する。
    static Vector3 GetMousePositionOnGround(Camera* camera, Input* input);

    static Vector3 CalcDirectionToMouseGround(
        const Vector3& startPosition,
        Camera* camera,
        Input* input);

    // プレイヤーの中心も使い、近くを狙った時の発射方向を安定させる。
    static Vector3 CalcDirectionToMouseAtHeight(
        const Vector3& startPosition,
        const Vector3& playerPosition,
        Camera* camera,
        Input* input,
        float targetHeight);

private:
    // 床ではなく、弾が狙う高さの平面へマウス位置を投影する。
    static Vector3 GetMousePositionOnPlane(Camera* camera, Input* input, float height);

    // 1フレームの移動区間へ、少し小さくした多層の発光粒子を並べる。
    void EmitTrail(const Vector3& start, const Vector3& end);

    // 弾の周囲へ短時間で消える火花を生成する
    void EmitSparks();

    // 壁の命中演出は、手前の敵に当たらなかった場合だけ生成する。
    void EmitWallImpact();

private:
    std::unique_ptr<Object3d> object_;

    // 弾が通過した位置へ軌跡を生成する共有パーティクルシステム
    ParticleSystem* particleSystem_ = nullptr;

    // 火花の生成間隔を管理するフレームカウンター
    int sparkFrame_ = 0;

    // 弾の位置
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    // 前の位置を残し、当たり判定と軌跡で同じ移動区間を使う。
    Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };
    // 壁で止まる前の終点と時刻を保存し、敵と壁を同じ時間軸で比べる。
    Vector3 movementEnd_ = { 0.0f, 0.0f, 0.0f };
    float wallHitTime_ = 1.0f;
    // 壁への命中と軌跡は、敵の判定が終わった後に一度だけ確定する。
    bool wallHitPending_ = false;
    bool effectsPending_ = false;

    // 弾の速度
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    // 狙う高さへ到達した後は、この高さを保って進む。床の下へ弾を落とさない。
    std::optional<float> flightHeight_;

    // 弾の大きさ
    Vector3 scale_ = { 0.5f, 0.5f, 0.5f };

    // 弾の当たり判定半径
    float colliderRadius_ = kColliderRadius;

    // 生存フレーム
    int lifeTime_ = 120;

    // 消すかどうか
    bool isDead_ = false;

    // 弾が参照するマップ情報
    const std::vector<LevelColliderData>* wallColliders_ = nullptr;

    // 1マスの大きさ
};
