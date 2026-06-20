#include "PlayerGrenade.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "../camera/Camera.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"

void PlayerGrenade::Initialize(
    Object3dCommon* object3dCommon,
    Camera* camera,
    const Vector3& position,
    const Vector3& throwDirection,
    const std::vector<LevelColliderData>* floorColliders,
    const std::vector<LevelColliderData>* wallColliders)
{
    // 投擲開始位置とコライダー参照を保存する
    position_ = position;
    floorColliders_ = floorColliders;
    wallColliders_ = wallColliders;

    // 水平方向へ進みながら上へ浮く初速度を作る
    velocity_ = {
        throwDirection.x * 0.22f,
        0.28f,
        throwDirection.z * 0.22f
    };

    // 新しいグレネードの状態へ戻す
    fuseTimer_ = 0;
    hasTouchedGround_ = false;
    isLanded_ = false;
    isExploded_ = false;
    isExplosionConsumed_ = false;
    isDead_ = false;

    // grenade.objを描画するObject3dを作る
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetCamera(camera);
    object_->SetModel("grenade/grenade.obj");
    object_->SetScale({ 0.24f, 0.24f, 0.24f });
    object_->SetRotate(rotation_);
    object_->SetTranslate(position_);
    object_->Update();
}

void PlayerGrenade::Update()
{
    if (!object_ || isDead_) {
        return;
    }


    const Vector3 previousPosition = position_;

    if (!isLanded_) {
        // 毎フレーム重力を加えて放物線を作る
        constexpr float kGravity = 0.012f;
        velocity_.y -= kGravity;

        // 速度を現在位置へ加算する
        position_.x += velocity_.x;
        position_.y += velocity_.y;
        position_.z += velocity_.z;

        // 飛んでいる間はモデルを回転させる
        rotation_.x += angularVelocity_.x;
        rotation_.y += angularVelocity_.y;
        rotation_.z += angularVelocity_.z;

        // 壁へ当たった場合は位置を戻して跳ね返す
        ResolveWallCollision(previousPosition);

        float groundY = 0.0f;
        if (GetGroundHeight(position_.x, position_.z, groundY)) {
            const float landingY = groundY + colliderRadius_;

            // 落下中に床の高さを下回ったときだけ接地させる
            if (velocity_.y <= 0.0f && position_.y <= landingY) {
                position_.y = landingY;

                // 最初に床へ触れた瞬間から爆発までの1秒を数え始める
                hasTouchedGround_ = true;

                // 速く落ちている間は小さく跳ねる
                if (velocity_.y < -0.055f) {
                    velocity_.y *= -0.38f;
                    velocity_.x *= 0.72f;
                    velocity_.z *= 0.72f;
                    angularVelocity_.x *= 0.82f;
                    angularVelocity_.y *= 0.82f;
                    angularVelocity_.z *= 0.82f;
                } else {
                    // 跳ね返りが小さくなったら床で停止させる
                    velocity_ = { 0.0f, 0.0f, 0.0f };
                    angularVelocity_ = { 0.0f, 0.0f, 0.0f };
                    isLanded_ = true;
                }
            }
        }
    }

    if (hasTouchedGround_) {
        // 床へ接触してから約1秒経過したら爆発させる
        fuseTimer_++;
        if (fuseTimer_ >= fuseDuration_) {
            Explode();
            return;
        }
    }

    // 計算したTransformを描画へ反映する
    object_->SetRotate(rotation_);
    object_->SetTranslate(position_);
    object_->Update();
}

void PlayerGrenade::Draw()
{
    if (!object_ || isDead_) {
        return;
    }

    // 爆発するまではグレネード本体を描画する
    object_->Draw();
}

bool PlayerGrenade::ConsumeExplosion(Vector3& explosionPosition)
{
    // 爆発前、またはすでに通知済みなら渡さない
    if (!isExploded_ || isExplosionConsumed_) {
        return false;
    }

    // 爆発したワールド座標をPlayerへ一度だけ渡す
    explosionPosition = position_;
    isExplosionConsumed_ = true;
    return true;
}
bool PlayerGrenade::GetGroundHeight(float x, float z, float& groundY) const
{
    if (!floorColliders_) {
        return false;
    }

    bool foundGround = false;
    float highestGroundY = -FLT_MAX;

    for (const LevelColliderData& collider : *floorColliders_) {
        // BOX型の床だけを接地対象にする
        if (!collider.hasCollider || collider.type != "BOX") {
            continue;
        }

        const float halfX = collider.size.x * 0.5f;
        const float halfY = collider.size.y * 0.5f;
        const float halfZ = collider.size.z * 0.5f;

        // グレネードのXZ座標が床の範囲外なら無視する
        if (x < collider.center.x - halfX ||
            x > collider.center.x + halfX ||
            z < collider.center.z - halfZ ||
            z > collider.center.z + halfZ) {
            continue;
        }

        const float topY = collider.center.y + halfY;
        if (!foundGround || topY > highestGroundY) {
            highestGroundY = topY;
            foundGround = true;
        }
    }

    if (foundGround) {
        groundY = highestGroundY;
    }

    return foundGround;
}

void PlayerGrenade::ResolveWallCollision(const Vector3& previousPosition)
{
    if (!wallColliders_) {
        return;
    }

    for (const LevelColliderData& collider : *wallColliders_) {
        // BOX型の壁だけを反射対象にする
        if (!collider.hasCollider || collider.type != "BOX") {
            continue;
        }

        const float halfX = collider.size.x * 0.5f;
        const float halfY = collider.size.y * 0.5f;
        const float halfZ = collider.size.z * 0.5f;

        const float minX = collider.center.x - halfX - colliderRadius_;
        const float maxX = collider.center.x + halfX + colliderRadius_;
        const float minY = collider.center.y - halfY - colliderRadius_;
        const float maxY = collider.center.y + halfY + colliderRadius_;
        const float minZ = collider.center.z - halfZ - colliderRadius_;
        const float maxZ = collider.center.z + halfZ + colliderRadius_;

        // 拡張した壁の範囲に入っていなければ衝突していない
        if (position_.x < minX || position_.x > maxX ||
            position_.y < minY || position_.y > maxY ||
            position_.z < minZ || position_.z > maxZ) {
            continue;
        }

        // 前フレームの位置から侵入した面を決め、対応する速度を反射する
        if (previousPosition.x <= minX) {
            position_.x = minX;
            velocity_.x = -std::fabs(velocity_.x) * 0.45f;
        } else if (previousPosition.x >= maxX) {
            position_.x = maxX;
            velocity_.x = std::fabs(velocity_.x) * 0.45f;
        } else if (previousPosition.z <= minZ) {
            position_.z = minZ;
            velocity_.z = -std::fabs(velocity_.z) * 0.45f;
        } else if (previousPosition.z >= maxZ) {
            position_.z = maxZ;
            velocity_.z = std::fabs(velocity_.z) * 0.45f;
        } else if (previousPosition.y >= maxY) {
            position_.y = maxY;
            velocity_.y = std::fabs(velocity_.y) * 0.30f;
        } else {
            // 面を特定できない場合は前の位置へ戻して減速させる
            position_ = previousPosition;
            velocity_.x *= -0.35f;
            velocity_.z *= -0.35f;
        }
    }
}

void PlayerGrenade::Explode()
{
    if (isExploded_) {
        return;
    }

    // 爆発状態へ移行し、モデルを描画対象から外す
    isExploded_ = true;
    isDead_ = true;

    // 後でここから爆発エフェクト、ダメージ、カメラシェイクを呼び出す
}