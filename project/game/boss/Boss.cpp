#include "Boss.h"

#include <algorithm>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../camera/Camera.h"

Boss::~Boss() = default;

void Boss::Initialize(
    Object3dCommon* object3dCommon,
    Camera* camera,
    const Vector3& position,
    const Vector3& rotation,
    const Vector3& scale)
{
    // Blenderから受け取ったTransformをボス側に保存する
    position_ = position;
    rotation_ = rotation;
    baseScale_ = scale;

    // ボスの大きさに合わせて、ざっくり当たり判定半径を広げる
    float maxScale = baseScale_.x;
    if (baseScale_.y > maxScale) {
        maxScale = baseScale_.y;
    }
    if (baseScale_.z > maxScale) {
        maxScale = baseScale_.z;
    }
    colliderRadius_ = maxScale * 1.1f;

    // 再生成時にHPと撃破状態を初期化する
    hp_ = maxHp_;
    isDead_ = false;
    isReadyToRemove_ = false;
    deathTimer_ = 0;

    // ボス専用の3Dオブジェクトを作る
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetCamera(camera);

    // 現時点では表示だけなので、固定でボスモデルを使う
    object_->SetModel("boss/boss.obj");
    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->SetScale(baseScale_);
    object_->Update();
}

void Boss::Update()
{
    if (!object_) {
        return;
    }

    if (isDead_) {
        // 撃破中は赤白に点滅しながら小さくする
        deathTimer_++;

        float deathRate = static_cast<float>(deathTimer_) / static_cast<float>(deathDuration_);
        deathRate = std::clamp(deathRate, 0.0f, 1.0f);

        const float scaleRate = 1.0f - deathRate;
        object_->SetScale({
            baseScale_.x * scaleRate,
            baseScale_.y * scaleRate,
            baseScale_.z * scaleRate
        });

        if ((deathTimer_ / 5) % 2 == 0) {
            object_->SetColor({ 1.0f, 0.1f, 0.05f, 1.0f });
        } else {
            object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }

        if (deathTimer_ >= deathDuration_) {
            isReadyToRemove_ = true;
        }
    } else {
        // 被弾で赤くなった色を次のフレームで通常色へ戻す
        object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // 移動はまだしないので、Transformだけ更新する
    object_->Update();
}

void Boss::Draw()
{
    if (!object_) {
        return;
    }

    // ボス本体を描画する
    object_->Draw();
}

SphereCollider Boss::GetCollider() const
{
    // 現在位置と半径からボス用の球コライダーを作る
    return { position_, colliderRadius_ };
}

void Boss::OnHit()
{
    if (isDead_) {
        return;
    }

    // 弾が当たるたびにHPを1減らす
    hp_--;

    // 被弾した瞬間だけ赤く光らせる
    if (object_) {
        object_->SetColor({ 1.0f, 0.25f, 0.15f, 1.0f });
    }

    if (hp_ <= 0) {
        // HPを0で止めて、撃破演出へ入る
        hp_ = 0;
        isDead_ = true;
        deathTimer_ = 0;
    }
}
