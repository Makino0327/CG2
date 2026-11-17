#include "Boss.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <random>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../../engine/3d/model/ModelManager.h"
#include "../../engine/particle/Particle.h"
#include "../camera/Camera.h"
#include "../../scene/LevelLoader.h"

namespace {
    // 大きさの違う破片を混ぜ、大きな塊から細かい肉片まで飛び散らせる。
    struct FragmentTier {
        int count;
        float minScale;   // ボスの大きさに対する比率
        float maxScale;
        float speedScale; // 小さいほど勢いよく遠くへ飛ぶ
        float bloodSize;  // 飛行中に垂らす血の粒の大きさ。0なら出さない
    };
    constexpr FragmentTier kFragmentTiers[] = {
        { 5, 0.15f, 0.24f, 0.75f, 0.55f },  // 大きな塊
        { 14, 0.08f, 0.14f, 1.0f, 0.35f },  // 中くらいの破片
        { 26, 0.025f, 0.07f, 1.35f, 0.0f }, // 細かい肉片
    };
}

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
    object3dCommon_ = object3dCommon;
    camera_ = camera;

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
    deathFragments_.clear();

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
        UpdateDeathEffect();
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
        object_->Update();
    }
}

void Boss::UpdateRenderOnly()
{
    // デバッグカメラ中は攻撃も撃破演出も進めない
    if (object_) {
        object_->Update();
    }
    for (auto& fragment : deathFragments_) { fragment.object->UpdateTransform(); }
}

void Boss::UpdateDeathEffect()
{
    if (isReadyToRemove_) { return; }
    ++deathTimer_;
    if (deathTimer_ < kDeathBurstFrame) {
        // 徐々に膨らみ、爆発が近づくほど揺れを強める。足元は床の高さに残す。
        const float progress = static_cast<float>(deathTimer_) / kDeathBurstFrame;
        const float swell = 1.0f + 0.43f * progress * progress;
        const float tremor = (0.025f + 0.18f * progress * progress) * colliderRadius_;
        const float phase = static_cast<float>(deathTimer_);
        object_->SetScale({ baseScale_.x * swell, baseScale_.y * swell, baseScale_.z * swell });
        object_->SetTranslate({ position_.x + std::sin(phase * 1.9f) * tremor,
            position_.y + baseScale_.y * (swell - 1.0f), position_.z + std::sin(phase * 2.7f) * tremor });
        object_->SetRotate({ rotation_.x + std::sin(phase * 1.5f) * 0.09f * progress,
            rotation_.y, rotation_.z + std::cos(phase * 2.3f) * 0.12f * progress });
        object_->SetColor({ 1.0f, 1.0f - 0.74f * progress, 1.0f - 0.90f * progress, 1.0f });
        object_->Update();
        return;
    }

    if (deathTimer_ == kDeathBurstFrame) { SpawnDeathFragments(); }

    // 通常の敵と同じく、重力で落として床で数回跳ね、時間が来たら縮めて消す。
    constexpr float kDeltaTime = 1.0f / 60.0f;
    constexpr float kGravity = 12.0f;
    const int fragmentFrame = deathTimer_ - kDeathBurstFrame;
    float shrinkRatio = 1.0f;
    if (fragmentFrame > kFragmentShrinkStartFrames) {
        shrinkRatio = 1.0f - static_cast<float>(fragmentFrame - kFragmentShrinkStartFrames) /
            static_cast<float>(kFragmentLifeFrames - kFragmentShrinkStartFrames);
        shrinkRatio = (std::max)(shrinkRatio, 0.0f);
    }
    for (auto& fragment : deathFragments_) {
        if (!fragment.landed) {
            const float previousHeight = fragment.position.y;
            fragment.velocity.y -= kGravity * kDeltaTime;
            fragment.position.x += fragment.velocity.x * kDeltaTime;
            fragment.position.y += fragment.velocity.y * kDeltaTime;
            fragment.position.z += fragment.velocity.z * kDeltaTime;
            fragment.rotation.x += fragment.spin.x * kDeltaTime;
            fragment.rotation.y += fragment.spin.y * kDeltaTime;
            fragment.rotation.z += fragment.spin.z * kDeltaTime;
            // 大きめの破片は、飛んでいる間に血の筋を引く。
            if (!fragment.hasSplashed && --fragment.trailTimer <= 0) {
                fragment.trailTimer = 3;
                EmitFragmentBlood(fragment.position, fragment.bloodSize, 1, false);
            }
            // OBJは中心から上下1の箱なので、Yの拡大率が床から中心までの高さになる。
            float floorY = 0.0f;
            if (FindFragmentFloor(fragment.position, previousHeight, floorY) &&
                fragment.position.y <= floorY + fragment.scale.y) {
                fragment.position.y = floorY + fragment.scale.y;
                if (!fragment.hasSplashed) {
                    // 最初に床へ叩きつけられた瞬間だけ、足元へ血を跳ねさせる。
                    fragment.hasSplashed = true;
                    EmitFragmentBlood({ fragment.position.x, floorY + 0.05f, fragment.position.z },
                        fragment.bloodSize, 6, true);
                }
                if (fragment.velocity.y < -1.2f) {
                    // 落下の勢いが残っている間は小さく跳ね返す
                    fragment.velocity.y *= -0.22f;
                    fragment.velocity.x *= 0.55f;
                    fragment.velocity.z *= 0.55f;
                } else {
                    fragment.velocity = {};
                    fragment.spin = {};
                    fragment.landed = true;
                }
            }
        }
        // 床との判定に使う大きさは変えず、描画だけを縮める。
        fragment.object->SetScale({ fragment.scale.x * shrinkRatio,
            fragment.scale.y * shrinkRatio, fragment.scale.z * shrinkRatio });
        fragment.object->SetTranslate(fragment.position);
        fragment.object->SetRotate(fragment.rotation);
        fragment.object->Update();
    }
    isReadyToRemove_ = deathTimer_ >= kDeathDuration;
}

void Boss::SpawnDeathFragments()
{
    // 通常の敵と同じく、本体のOBJを小さくした破片を中心付近からランダムに飛ばす。
    // ボスは体が大きいので、出現範囲・飛ぶ速さ・破片の大きさを体格に合わせて広げる。
    const Vector3 center = object_->GetTranslate();
    const Vector3 bodyScale = object_->GetScale();
    const float bodySize = (std::max)({ std::abs(bodyScale.x), std::abs(bodyScale.y), std::abs(bodyScale.z), 1.0f });
    const float speedScale = std::sqrt(bodySize);
    static std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> offsetDistribution(-0.45f, 0.45f);
    std::uniform_real_distribution<float> sideVelocityDistribution(-2.8f, 2.8f);
    std::uniform_real_distribution<float> upVelocityDistribution(3.5f, 6.0f);
    std::uniform_real_distribution<float> rotationDistribution(-3.14f, 3.14f);
    std::uniform_real_distribution<float> angularDistribution(-7.0f, 7.0f);
    std::uniform_real_distribution<float> stretchDistribution(0.6f, 1.35f);
    deathFragments_.reserve(std::size(kFragmentTiers) * 26);
    for (const FragmentTier& tier : kFragmentTiers) {
        std::uniform_real_distribution<float> scaleDistribution(tier.minScale, tier.maxScale);
        for (int index = 0; index < tier.count; ++index) {
            auto& fragment = deathFragments_.emplace_back();
            fragment.position = {
                center.x + offsetDistribution(randomEngine) * bodyScale.x,
                center.y + offsetDistribution(randomEngine) * bodyScale.y,
                center.z + offsetDistribution(randomEngine) * bodyScale.z,
            };
            const float speed = speedScale * tier.speedScale;
            fragment.velocity = {
                sideVelocityDistribution(randomEngine) * speed,
                upVelocityDistribution(randomEngine) * speed,
                sideVelocityDistribution(randomEngine) * speed,
            };
            fragment.rotation = {
                rotationDistribution(randomEngine),
                rotationDistribution(randomEngine),
                rotationDistribution(randomEngine),
            };
            // 小さい欠片ほど速く回し、大きな塊には重さを残す。
            fragment.spin = {
                angularDistribution(randomEngine) * tier.speedScale,
                angularDistribution(randomEngine) * tier.speedScale,
                angularDistribution(randomEngine) * tier.speedScale,
            };
            // 軸ごとに伸び縮みさせ、同じOBJでも形の違う肉片に見せる。
            const float fragmentScale = scaleDistribution(randomEngine);
            fragment.scale = {
                fragmentScale * stretchDistribution(randomEngine) * bodyScale.x,
                fragmentScale * stretchDistribution(randomEngine) * 0.7f * bodyScale.y,
                fragmentScale * stretchDistribution(randomEngine) * bodyScale.z,
            };
            fragment.bloodSize = tier.bloodSize * bodySize * 0.35f;
            fragment.trailTimer = static_cast<int>(unit(randomEngine) * 3.0f);
            fragment.object = std::make_unique<Object3d>();
            fragment.object->Initialize(object3dCommon_);
            fragment.object->SetCamera(camera_);
            fragment.object->SetModel("boss/boss.obj");
            // 血に濡れた肉片を混ぜ、暗い赤みの差で破片の見分けをつける。
            const float wet = unit(randomEngine);
            fragment.object->SetColor({ 1.0f - 0.25f * wet, 1.0f - 0.65f * wet, 1.0f - 0.68f * wet, 1.0f });
            fragment.object->SetScale(fragment.scale);
            fragment.object->SetRotate(fragment.rotation);
            fragment.object->SetTranslate(fragment.position);
            fragment.object->Update();
        }
    }
}

void Boss::EmitFragmentBlood(const Vector3& position, float size, int count, bool splash)
{
    if (!bloodParticleSystem_ || size <= 0.0f) { return; }
    static std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> side(-1.0f, 1.0f);
    for (int index = 0; index < count; ++index) {
        // 粒ごとに大きさを変え、細かい飛沫と大きなしずくを混ぜる。
        const float dropSize = size * (0.4f + 0.9f * unit(randomEngine));
        const Vector3 velocity = splash
            ? Vector3{ side(randomEngine) * 3.2f, 0.6f + 1.8f * unit(randomEngine), side(randomEngine) * 3.2f }
            : Vector3{ side(randomEngine) * 0.5f, -0.3f - 0.8f * unit(randomEngine), side(randomEngine) * 0.5f };
        const Vector4 color = (index % 2 == 0)
            ? Vector4{ 0.55f, 0.01f, 0.005f, 0.75f }
            : Vector4{ 0.32f, 0.004f, 0.003f, 0.82f };
        bloodParticleSystem_->Emit(position, { dropSize, dropSize, dropSize }, velocity, color,
            (splash ? 0.5f : 0.35f) + 0.4f * unit(randomEngine));
    }
}

bool Boss::FindFragmentFloor(const Vector3& position, float previousHeight, float& floorY) const
{
    if (!floorColliders_) { return false; }
    // 破片の足元にある床のうち、前フレームの中心より低い最上面を選ぶ。
    floorY = std::numeric_limits<float>::lowest();
    for (const auto& floor : *floorColliders_) {
        if (!floor.hasCollider || floor.type != "BOX" ||
            std::abs(position.x - floor.center.x) > floor.size.x * 0.5f ||
            std::abs(position.z - floor.center.z) > floor.size.z * 0.5f) { continue; }
        const float top = floor.center.y + floor.size.y * 0.5f;
        if (top <= previousHeight + 0.05f) { floorY = (std::max)(floorY, top); }
    }
    return floorY != std::numeric_limits<float>::lowest();
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
    if (deathTimer_ >= kDeathBurstFrame) {
        for (auto& fragment : deathFragments_) { fragment.object->Draw(); }
        return;
    }
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
