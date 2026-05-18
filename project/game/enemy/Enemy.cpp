#include "Enemy.h"

#include "../../engine/3d/obj3d/Object3dCommon.h"

void Enemy::Initialize(Object3dCommon* object3dCommon, const Vector3& position)
{
    // 初期座標を保存する
    position_ = position;

    // 敵の3Dオブジェクトを作る
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);

    // 敵モデルを設定する
    object_->SetModel("enemy/enemy.obj");

    // 初期状態を反映する
    object_->SetScale(scale_);
    object_->SetRotate(rotation_);
    object_->SetTranslate(position_);
}

void Enemy::Update()
{
    if (!object_ || isDead_) {
        return;
    }
    // 敵からターゲットへの向きを作る
    Vector3 direction = {
        targetPosition_.x - position_.x,
        0.0f,
        targetPosition_.z - position_.z
    };

    // 正規化して少しずつ前進する
    direction = Normalize(direction);

    position_.x += direction.x * moveSpeed_;
    position_.z += direction.z * moveSpeed_;
    // 移動方向を向くように回転する
    if (direction.x != 0.0f || direction.z != 0.0f) {
        rotation_.y = std::atan2(direction.x, direction.z);
    }

    // 現在の状態をオブジェクトに反映する
    object_->SetScale(scale_);
    object_->SetRotate(rotation_);
    object_->SetTranslate(position_);
    object_->Update();
}

void Enemy::Draw()
{
    if (!object_ || isDead_) {
        return;
    }

    // 敵を描画する
    object_->Draw();
}

void Enemy::SetPosition(const Vector3& position)
{
    // 外から敵の座標を変えられるようにする
    position_ = position;
}

Vector3 Enemy::GetWorldPosition() const
{
    // 現在の座標を返す
    return position_;
}
SphereCollider Enemy::GetCollider() const
{
    // 敵の現在位置を球の当たり判定として返す
    return { position_, colliderRadius_ };
}

void Enemy::OnHit()
{
    // 弾が当たった敵は倒された扱いにする
    isDead_ = true;
}

void Enemy::SetTargetPosition(const Vector3& targetPosition)
{
    // 追いかける対象の座標を保存する
    targetPosition_ = targetPosition;
}
