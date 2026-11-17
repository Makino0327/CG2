#include "Boss.h"

#include <algorithm>
#include <cmath>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../camera/Camera.h"
#include "../../scene/LevelLoader.h"

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

    // レベル再読み込み時に攻撃途中の状態を持ち越さない
    action_ = Action::Chase;
    actionTimer_ = 0;
    attackCount_ = 0;
    stompAttack_ = false;
    impactThisFrame_ = false;
    attackCenter_ = position_;
    attackRadius_ = 0.0f;
    walkPhase_ = 0.0f;
    charge_.Reset();
    chargeCooldown_ = 0;
    chargeRecovery_ = false;

    // ボス専用の3Dオブジェクトを作る
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetCamera(camera);

    // 巨大ゾンビのモデルを使用する
    object_->SetModel("boss/boss.obj");
    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->SetScale(baseScale_);
    object_->Update();

    // 赤い帯と矢印を、床の少し上へ表示する専用モデル。
    chargeWarning_ = std::make_unique<Object3d>();
    chargeWarning_->Initialize(object3dCommon);
    chargeWarning_->SetCamera(camera);
    chargeWarning_->SetModel("boss/charge_warning.obj");
}

void Boss::Update(const Vector3& targetPosition, bool canAttack)
{
    charge_.BeginFrame();
    // 攻撃判定は着地した瞬間だけ発生させ、連続ダメージを防ぐ
    impactThisFrame_ = false;
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
        if (chargeCooldown_ > 0) { --chargeCooldown_; }
        // 描画だけを揺らし、当たり判定の中心は地上の移動位置に保つ
        Vector3 drawPosition = position_;
        Vector3 drawRotation = rotation_;
        Vector3 drawScale = baseScale_;
        object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        if (!canAttack) {
            // プレイヤー死亡後は追跡と攻撃を中止する
            action_ = Action::Chase;
            actionTimer_ = 0;
            charge_.Reset();
            chargeRecovery_ = false;
        } else if (action_ == Action::Chase) {
            const float dx = targetPosition.x - position_.x;
            const float dz = targetPosition.z - position_.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance > 0.001f) {
                // 向きを少しずつ変え、巨体の重さを表現する
                const float yawDifference = std::remainder(
                    std::atan2(dx, dz) - rotation_.y, 6.2831853f);
                rotation_.y += std::clamp(yawDifference, -0.035f, 0.035f);
            }
            if (distance <= colliderRadius_ + 4.0f) {
                BeginAttack(targetPosition);
            } else if (distance >= 14.0f && chargeCooldown_ == 0 && BeginCharge(targetPosition)) {
                // 遠い相手には、追跡の代わりに1秒の予告付き突進を選ぶ。
            } else {
                MoveTowards(targetPosition);
                walkPhase_ += 0.09f;
            }
            drawPosition = position_;
            drawRotation = rotation_;
            drawRotation.z += std::sin(walkPhase_) * 0.045f;
            drawPosition.y += std::abs(std::sin(walkPhase_)) * 0.12f;
        } else if (action_ == Action::Charge) {
            const bool windingUp = charge_.IsWindingUp();
            charge_.Update();
            position_ = charge_.GetPosition();
            drawPosition = position_;
            drawRotation = rotation_;
            // 溜めでは身を引き、走り出したら前傾して勢いを見せる。
            drawRotation.x += windingUp ? -0.2f * charge_.GetWindupProgress() : 0.3f;
            if (windingUp) {
                object_->SetColor({ 1.0f, 0.25f, 0.15f, 1.0f });
            } else {
                walkPhase_ += 0.55f;
                drawPosition.y += std::abs(std::sin(walkPhase_)) * 0.2f;
            }
            if (charge_.IsFinished()) {
                action_ = Action::Recovery;
                actionTimer_ = 0;
                chargeRecovery_ = true;
                chargeCooldown_ = 180;
            }
        } else if (action_ == Action::Windup) {
            // 叩きつけは0.9秒、踏みつけは1.2秒溜めてから攻撃する。
            const int windupFrames = stompAttack_ ? 72 : 54;
            const float progress = static_cast<float>(++actionTimer_) /
                static_cast<float>(windupFrames);
            drawRotation.x -= progress * (stompAttack_ ? 0.12f : 0.35f);
            drawPosition.y += progress * (stompAttack_ ? 0.9f : 0.3f);
            object_->SetColor({ 1.0f, 1.0f - progress * 0.7f, 1.0f - progress * 0.8f, 1.0f });
            if (actionTimer_ >= windupFrames) {
                // 溜め終わりに一度だけ範囲攻撃を発生させる
                impactThisFrame_ = true;
                action_ = Action::Recovery;
                actionTimer_ = 0;
                drawPosition = position_;
                drawRotation = rotation_;
                drawRotation.x += stompAttack_ ? 0.12f : 0.45f;
                drawScale.y *= 0.8f;
                object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            }
        } else {
            // 攻撃後の隙を短くし、叩きつけは1秒、踏みつけは1.4秒で次の行動へ戻る。
            const int recoveryFrames = chargeRecovery_ ? 90 : (stompAttack_ ? 84 : 60);
            const float recovery = 1.0f - std::clamp(
                static_cast<float>(++actionTimer_) / 24.0f, 0.0f, 1.0f);
            drawRotation.x += recovery * (stompAttack_ ? 0.12f : 0.45f);
            drawScale.y *= 1.0f - recovery * 0.2f;
            if (actionTimer_ >= recoveryFrames) {
                action_ = Action::Chase;
                actionTimer_ = 0;
                chargeRecovery_ = false;
                charge_.Reset();
            }
        }

        // 行動と攻撃姿勢をモデルへ反映する
        object_->SetTranslate(drawPosition);
        object_->SetRotate(drawRotation);
        object_->SetScale(drawScale);
    }

    // 移動・攻撃・撃破演出を描画用の行列へ反映する
    object_->Update();
}

void Boss::UpdateRenderOnly()
{
    // デバッグカメラ中は攻撃も撃破演出も進めない
    if (object_) {
        object_->Update();
    }
}

void Boss::MoveTowards(const Vector3& targetPosition)
{
    const float dx = targetPosition.x - position_.x;
    const float dz = targetPosition.z - position_.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= 0.001f) {
        return;
    }

    // 球の半径を考慮して壁を広げ、ボスの中心が入らないようにする
    const auto canMove = [this](const Vector3& next) {
        if (!wallColliders_) {
            return true;
        }
        for (const LevelColliderData& wall : *wallColliders_) {
            if (!wall.hasCollider || wall.type != "BOX") {
                continue;
            }
            if (std::abs(next.y - wall.center.y) < wall.size.y * 0.5f + colliderRadius_ &&
                std::abs(next.x - wall.center.x) < wall.size.x * 0.5f + colliderRadius_ &&
                std::abs(next.z - wall.center.z) < wall.size.z * 0.5f + colliderRadius_) {
                return false;
            }
        }
        return true;
    };

    // XとZを別々に判定して、斜めに壁へ接触しても滑るように移動する
    const float step = (std::min)(distance, moveSpeed_);
    Vector3 next = position_;
    next.x += dx / distance * step;
    if (canMove(next)) {
        position_ = next;
    }
    next = position_;
    next.z += dz / distance * step;
    if (canMove(next)) {
        position_ = next;
    }
}

void Boss::BeginAttack(const Vector3& targetPosition)
{
    chargeRecovery_ = false;
    charge_.Reset();
    action_ = Action::Windup;
    actionTimer_ = 0;
    stompAttack_ = (++attackCount_ % 3 == 0);

    // 攻撃開始時に向きと攻撃地点を固定する。溜め中はプレイヤーを追尾しない
    const float dx = targetPosition.x - position_.x;
    const float dz = targetPosition.z - position_.z;
    if (dx * dx + dz * dz > 0.000001f) {
        rotation_.y = std::atan2(dx, dz);
    }
    attackCenter_ = position_;
    // モデルの足元に予告円を置く。現在のボスステージは平らな床を使用する
    attackCenter_.y -= std::abs(baseScale_.y) - 0.1f;
    attackRadius_ = stompAttack_ ? colliderRadius_ + 4.5f : colliderRadius_ + 0.5f;
    if (!stompAttack_) {
        // 両腕の叩きつけはボスの前方に着地する
        const float reach = colliderRadius_ + 1.0f;
        attackCenter_.x += std::sin(rotation_.y) * reach;
        attackCenter_.z += std::cos(rotation_.y) * reach;
    }
}

bool Boss::BeginCharge(const Vector3& targetPosition)
{
    // 壁で進めない場合は通常の追跡へ戻し、次の再判定まで少し間を空ける。
    chargeCooldown_ = 60;
    if (!charge_.Begin(position_, targetPosition, colliderRadius_, wallColliders_)) { return false; }
    action_ = Action::Charge;
    actionTimer_ = 0;
    stompAttack_ = false;
    chargeRecovery_ = false;
    rotation_.y = std::atan2(charge_.GetDirection().x, charge_.GetDirection().z);
    return true;
}

void Boss::DrawAttackWarning()
{
    if (isDead_ || action_ != Action::Charge || !charge_.IsWindingUp() || !chargeWarning_) { return; }
    const Vector3& direction = charge_.GetDirection();
    const Vector3& start = charge_.GetStart();
    const Vector3& end = charge_.GetEnd();
    const float length = std::hypot(end.x - start.x, end.z - start.z);
    // 本体の半径を含む通過範囲を示し、予告の外へ横に避けられるようにする。
    chargeWarning_->SetTranslate({ start.x - direction.x * colliderRadius_,
        position_.y - std::abs(baseScale_.y) + 0.07f, start.z - direction.z * colliderRadius_ });
    chargeWarning_->SetRotate({ 0.0f, rotation_.y, 0.0f });
    chargeWarning_->SetScale({ colliderRadius_, 1.0f, length + colliderRadius_ * 2.0f });
    chargeWarning_->SetColor({ 1.0f, 0.08f, 0.02f, 0.25f + charge_.GetWindupProgress() * 0.2f });
    chargeWarning_->UpdateTransform();
    chargeWarning_->Draw();
}

bool Boss::IsAttackHit(const SphereCollider& target)
{
    if (isDead_) { return false; }
    // 終点へ到着したフレームも含め、突進した移動区間で命中を判定する。
    if (action_ == Action::Charge || chargeRecovery_) { return charge_.TryHit(target); }
    if (!IsImpactFrame()) {
        return false;
    }

    // 予告円と同じXZ範囲を使い、高さが大きく離れた相手には当てない
    const float dx = target.center.x - attackCenter_.x;
    const float dz = target.center.z - attackCenter_.z;
    const float radius = attackRadius_ + target.radius;
    return dx * dx + dz * dz <= radius * radius &&
        std::abs(target.center.y - attackCenter_.y) <= 3.0f + target.radius;
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
        // 撃破と着地が同じフレームなら、ボスの攻撃を取り消す
        impactThisFrame_ = false;
    }
}
