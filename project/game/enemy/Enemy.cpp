#include "Enemy.h"
#include <cfloat>
#include <cmath>
#include <random>

#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../../engine/particle/Particle.h"

namespace {
    // a から b を引いたベクトルを返す
    Vector3 SubtractVector3(const Vector3& a, const Vector3& b) {
        return {
            a.x - b.x,
            a.y - b.y,
            a.z - b.z
        };
    }

    // 2つのベクトルの内積を返す
    float DotVector3(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

void Enemy::Initialize(Object3dCommon* object3dCommon, Camera* camera, const Vector3& position)
{
    // 死亡時の破片生成に使用する情報を保存する
    object3dCommon_ = object3dCommon;
    camera_ = camera;

    // 再生成時はHPと死亡状態を初期値へ戻す
    hp_ = maxHp_;
    isDead_ = false;

    // 再生成時はプレイヤー発見状態も解除する
    hasDetectedPlayer_ = false;

    // 死亡演出の状態も初期化する
    deathEffectTimer_ = 0.0f;
    isReadyToRemove_ = false;

    // 初期位置を設定する
    position_ = position;

    // WaypointMover を初期化する
    waypointMover_.Initialize(object3dCommon, camera);
    waypointMover_.SetModel("enemy/enemy.obj");
    waypointMover_.SetScale(scale_);
    waypointMover_.SetRotation(rotation_);
    waypointMover_.SetPosition(position_);
    waypointMover_.SetMoveSpeed(moveSpeed_);
    waypointMover_.SetLoop(true);

    // 初期状態を反映する
    waypointMover_.Update();
}

void Enemy::Update()
{
    // 死亡後は通常移動を止め、破片だけを更新する
    if (isDead_) {
        UpdateDeathFragments();
        return;
    }

    Object3d* object = waypointMover_.GetObject3d();
    if (!object) {
        return;
    }

    // 前フレームの状態を保存する
    prevPosition_ = position_;
    wasChasing_ = isChasing_;

    // 次に使う位置を現在の描画位置から取得する
    Vector3 nextPosition = object->GetTranslate();

    // プレイヤーまでのXZ方向ベクトルを求める
    Vector3 toPlayer = {
        targetPosition_.x - position_.x,
        0.0f,
        targetPosition_.z - position_.z
    };

    // プレイヤーまでのXZ距離を求める
    float distanceToPlayer = std::sqrt(
        toPlayer.x * toPlayer.x +
        toPlayer.z * toPlayer.z
    );
    isTargetInSight_ = CheckTargetInSight();

    // 視界に入ったら追跡を開始する
    if (!isChasing_ && isTargetInSight_) {
        // 追跡開始フラグを立てる
        isChasing_ = true;

        // 一度発見したことを記録する
        // このフラグは巡回へ戻っても解除しない
        hasDetectedPlayer_ = true;
    }

    // 一定距離以上離れたら追跡をやめる
    if (isChasing_ && distanceToPlayer > chaseKeepRange_) {
        // 追跡終了フラグを下ろす
        isChasing_ = false;
    }

    // 追跡終了時は巡回へ戻す
    if (wasChasing_ && !isChasing_) {
        // 巡回ルートへ復帰する
        waypointMover_.ResumePatrol();
    }

    if (isChasing_ && distanceToPlayer > 0.001f) {
        // 追跡中は音への警戒を打ち切って追跡を優先する
        hearingTimer_ = 0;

        // 追跡方向の単位ベクトルを求める
        Vector3 direction = Normalize(toPlayer);

        // 追跡方向へ移動する
        nextPosition.x += direction.x * moveSpeed_;
        nextPosition.z += direction.z * moveSpeed_;

        // 追跡方向を向く
        rotation_.y = std::atan2(direction.x, direction.z);
    } else if (hearingTimer_ > 0) {
        // 音を聞いたときは移動を止めて、音のした方向へその場で振り向く
        hearingTimer_--;

        const Vector3 toSound = {
            heardSoundPosition_.x - position_.x,
            0.0f,
            heardSoundPosition_.z - position_.z
        };

        if (std::fabs(toSound.x) > 0.001f || std::fabs(toSound.z) > 0.001f) {
            const float targetYaw = std::atan2(toSound.x, toSound.z);

            // 最短の回転方向を求めて一定速度で旋回する
            constexpr float kPi = 3.14159265f;
            float yawDiff = targetYaw - rotation_.y;
            while (yawDiff > kPi) { yawDiff -= kPi * 2.0f; }
            while (yawDiff < -kPi) { yawDiff += kPi * 2.0f; }

            if (std::fabs(yawDiff) <= hearingTurnSpeed_) {
                rotation_.y = targetYaw;
            } else {
                rotation_.y +=
                    (yawDiff > 0.0f) ? hearingTurnSpeed_ : -hearingTurnSpeed_;
            }
        }

        // 警戒が終わったら巡回へ戻る
        if (hearingTimer_ == 0) {
            waypointMover_.ResumePatrol();
        }
    } else {
        // 追跡していないときは巡回を更新する
        waypointMover_.Update();

        // 巡回更新後の位置と回転を反映する
        nextPosition = object->GetTranslate();
        rotation_ = object->GetRotate();
    }

    // 床コライダーで高さを補正する
    ResolveGroundHeight(nextPosition);

    // 壁コライダーで衝突を解決する
    ResolveWallCollision(nextPosition);

    // 計算後の位置を確定する
    position_ = nextPosition;

    // transform を更新して描画へ反映する
    object->SetScale(scale_);
    object->SetRotate(rotation_);
    object->SetTranslate(position_);
    object->Update();
}

void Enemy::Draw()
{
    // 生存中は通常の敵モデルを描画する
    if (!isDead_) {
        waypointMover_.Draw();
        return;
    }

    // 死亡後は敵モデルの代わりに破片を描画する
    for (DeathFragment& fragment : deathFragments_) {
        if (fragment.object) {
            fragment.object->Draw();
        }
    }
}

void Enemy::SetPosition(const Vector3& position)
{
    // 補正後の位置を設定する
    position_ = position;

    // 内部 Object3d の位置も更新する
    Object3d* object = waypointMover_.GetObject3d();
    if (object) {
        object->SetTranslate(position_);
    }
}

Vector3 Enemy::GetWorldPosition() const
{
    // 現在位置を返す
    return position_;
}

void Enemy::OnHearSound(const Vector3& soundPosition)
{
    // 死亡中は反応しない
    if (isDead_) {
        return;
    }

    // 追跡中はすでにプレイヤーを狙っているので音には反応しない
    if (isChasing_) {
        return;
    }

    // 音の位置を記録して、しばらく音の方向を警戒する(60fpsで約3秒)
    heardSoundPosition_ = soundPosition;
    hearingTimer_ = 180;
}

SphereCollider Enemy::GetCollider() const
{
    // 位置と半径から球コライダーを作る
    return { position_, colliderRadius_ };
}

void Enemy::OnHit(const Vector3& hitPosition, const Vector3& hitDirection)
{
    if (isDead_) {
        return;
    }

    // 弾が進んできた方向へ血しぶきを飛ばす
    EmitBloodSplatter(hitPosition, hitDirection);

    // 被弾するたびにHPを1減らす
    hp_--;

    // HPが0になったときだけ撃破扱いにする
    if (hp_ <= 0) {
        // HPを0で止める
        hp_ = 0;

        // 死亡状態にして、通常の敵モデルを非表示にする
        isDead_ = true;

        // 大きな血しぶきとOBJ破片を発生させる
        StartDeathEffect();
    }
}

void Enemy::OnExplosionHit()
{
    if (isDead_) {
        return;
    }

    // 通常HPに関係なく、その場で撃破状態へ移行する
    hp_ = 0;
    isDead_ = true;

    // 通常死亡と同じOBJ破片と血しぶき演出を開始する
    StartDeathEffect();
}
void Enemy::OnMeleeHit()
{
    // すでに死亡している敵には何もしない
    if (isDead_) {
        return;
    }

    // 近接攻撃は残りHPに関係なく即死させる
    hp_ = 0;
    isDead_ = true;

    // 通常撃破と同じ血しぶきと破片を発生させる
    StartDeathEffect();
}

void Enemy::OnMeleeDamage(const Vector3& hitPosition, const Vector3& hitDirection)
{
    // すでに死亡している敵には何もしない
    if (isDead_) {
        return;
    }

    // 斬られた位置から血しぶきを出す
    EmitBloodSplatter(hitPosition, hitDirection);

    // 通常近接攻撃は1ダメージだけ与える
    hp_--;

    // HPが0になったときだけ撃破扱いにする
    if (hp_ <= 0) {
        // HPを0で止める
        hp_ = 0;

        // 死亡状態にして、通常の敵モデルを非表示にする
        isDead_ = true;

        // 通常撃破と同じ血しぶきと破片を発生させる
        StartDeathEffect();
    }
}

void Enemy::EmitBloodSplatter(
    const Vector3& hitPosition,
    const Vector3& hitDirection)
{
    if (!bloodParticleSystem_) {
        return;
    }

    // 弾の進行方向とは逆側へ血しぶきを噴き出させる
    Vector3 reverseHitDirection = {
        -hitDirection.x,
        -hitDirection.y,
        -hitDirection.z
    };
    Vector3 forward = Normalize(reverseHitDirection);
    Vector3 side = { -forward.z, 0.0f, forward.x };
    // 弾が実際に当たった敵コライダー表面から血しぶきを出す
    Vector3 bloodPosition = hitPosition;

    // 被弾方向を中心に密度の高い扇状の血しぶきを作る
    constexpr int kBloodParticleCount = 22;

    // 粒子が同じ位置に重ならないよう発生位置をランダムに散らす
    static std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> sideOffsetDistribution(-0.38f, 0.38f);
    std::uniform_real_distribution<float> heightOffsetDistribution(-0.28f, 0.38f);
    std::uniform_real_distribution<float> depthOffsetDistribution(-0.12f, 0.22f);
    std::uniform_real_distribution<float> sizeDistribution(0.55f, 1.0f);

    for (int index = 0; index < kBloodParticleCount; ++index) {
        float ratio = static_cast<float>(index) /
            static_cast<float>(kBloodParticleCount - 1);

        // -1.0から1.0へ変化させて左右へ広げる
        float sideRatio = ratio * 2.0f - 1.0f;
        float forwardPower = 1.2f + 0.18f * static_cast<float>(index % 6);
        float sidePower = sideRatio * (0.65f + 0.12f * static_cast<float>(index % 5));
        float upwardPower = 0.25f + 0.16f * static_cast<float>(index % 7);

        Vector3 velocity = {
            forward.x * forwardPower + side.x * sidePower,
            upwardPower,
            forward.z * forwardPower + side.z * sidePower
        };

        float size = sizeDistribution(randomEngine);
        float lifeTime = 0.28f + 0.045f * static_cast<float>(index % 6);

        float sideOffset = sideOffsetDistribution(randomEngine);
        float heightOffset = heightOffsetDistribution(randomEngine);
        float depthOffset = depthOffsetDistribution(randomEngine);

        Vector3 spawnPosition = {
            bloodPosition.x + side.x * sideOffset + forward.x * depthOffset,
            bloodPosition.y + heightOffset,
            bloodPosition.z + side.z * sideOffset + forward.z * depthOffset
        };

        Vector4 color{};
        if ((index % 3) == 0) {
            color = { 0.30f, 0.008f, 0.006f, 0.55f };
        } else if ((index % 3) == 1) {
            color = { 0.52f, 0.014f, 0.008f, 0.50f };
        } else {
            color = { 0.68f, 0.025f, 0.012f, 0.45f };
        }

        bloodParticleSystem_->Emit(
            spawnPosition,
            { size, size, size },
            velocity,
            color,
            lifeTime);
    }

    // 命中箇所へ大きめの濃い血の塊を重ねる
    bloodParticleSystem_->Emit(
        bloodPosition,
        { 1.1f, 1.1f, 1.1f },
        { forward.x * 0.7f, 0.3f, forward.z * 0.7f },
        { 0.38f, 0.008f, 0.005f, 0.48f },
        0.22f);
}

void Enemy::StartDeathEffect()
{
    // 死亡演出の時間を最初から開始する
    deathEffectTimer_ = 0.0f;
    isReadyToRemove_ = false;

    // 敵の中心から通常より大きな血しぶきを出す
    EmitDeathBurst();

    // 毎回同じ飛び方にならないよう乱数を用意する
    static std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> offsetDistribution(-0.45f, 0.45f);
    std::uniform_real_distribution<float> sideVelocityDistribution(-2.8f, 2.8f);
    std::uniform_real_distribution<float> upVelocityDistribution(3.5f, 6.0f);
    std::uniform_real_distribution<float> rotationDistribution(-3.14f, 3.14f);
    std::uniform_real_distribution<float> angularDistribution(-7.0f, 7.0f);
    std::uniform_real_distribution<float> scaleDistribution(0.18f, 0.32f);

    for (DeathFragment& fragment : deathFragments_) {
        // enemy.objを小さく表示するObject3dを作る
        fragment.object = std::make_unique<Object3d>();
        fragment.object->Initialize(object3dCommon_);
        fragment.object->SetCamera(camera_);
        fragment.object->SetModel("enemy/enemy.obj");

        // 敵の中心付近から破片を発生させる
        fragment.position = {
            position_.x + offsetDistribution(randomEngine),
            position_.y + offsetDistribution(randomEngine),
            position_.z + offsetDistribution(randomEngine)
        };

        // 横方向へ散らしながら上へ飛ばす
        fragment.velocity = {
            sideVelocityDistribution(randomEngine),
            upVelocityDistribution(randomEngine),
            sideVelocityDistribution(randomEngine)
        };

        // 落下中に破片を回転させる
        fragment.rotation = {
            rotationDistribution(randomEngine),
            rotationDistribution(randomEngine),
            rotationDistribution(randomEngine)
        };
        fragment.angularVelocity = {
            angularDistribution(randomEngine),
            angularDistribution(randomEngine),
            angularDistribution(randomEngine)
        };

        // 同じOBJでも大きさを少し変えて破片らしく見せる
        const float fragmentScale = scaleDistribution(randomEngine);
        fragment.scale = {
            fragmentScale,
            fragmentScale * 0.7f,
            fragmentScale
        };
        fragment.isLanded = false;

        // 作成直後のTransformを反映する
        fragment.object->SetScale(fragment.scale);
        fragment.object->SetRotate(fragment.rotation);
        fragment.object->SetTranslate(fragment.position);
        fragment.object->Update();
    }
}

void Enemy::EmitDeathBurst()
{
    if (!bloodParticleSystem_) {
        return;
    }

    // 敵モデルのおおよその中心を発生位置にする
    Vector3 centerPosition = position_;
    centerPosition.y += 0.25f;

    // 通常被弾より多い粒子を使い、死亡時の塊を作る
    constexpr int kDeathParticleCount = 48;

    static std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> directionDistribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> upwardDistribution(0.4f, 2.8f);
    std::uniform_real_distribution<float> sizeDistribution(1.1f, 2.0f);
    std::uniform_real_distribution<float> lifeDistribution(0.75f, 1.25f);
    std::uniform_real_distribution<float> positionDistribution(-0.3f, 0.3f);

    for (int index = 0; index < kDeathParticleCount; ++index) {
        // 中心から全方向へ広がる速度を作る
        Vector3 velocity = {
            directionDistribution(randomEngine) * 2.8f,
            upwardDistribution(randomEngine),
            directionDistribution(randomEngine) * 2.8f
        };

        // 完全に一点へ重ならないよう中心付近で少しだけ散らす
        Vector3 spawnPosition = {
            centerPosition.x + positionDistribution(randomEngine),
            centerPosition.y + positionDistribution(randomEngine),
            centerPosition.z + positionDistribution(randomEngine)
        };

        const float size = sizeDistribution(randomEngine);
        const float lifeTime = lifeDistribution(randomEngine);

        // 暗い赤と明るい赤を混ぜて立体感を出す
        Vector4 color = (index % 2 == 0)
            ? Vector4{ 0.55f, 0.01f, 0.005f, 0.65f }
            : Vector4{ 0.82f, 0.025f, 0.008f, 0.55f };

        bloodParticleSystem_->Emit(
            spawnPosition,
            { size, size, size },
            velocity,
            color,
            lifeTime);
    }

    // 中央に大きく、少し長く残る血の塊を追加する
    bloodParticleSystem_->Emit(
        centerPosition,
        { 3.0f, 3.0f, 3.0f },
        { 0.0f, 0.25f, 0.0f },
        { 0.42f, 0.005f, 0.003f, 0.72f },
        1.35f);
}

void Enemy::UpdateDeathFragments()
{
    // 現在のEnemy::Updateがフレーム単位なので60FPS相当で計算する
    constexpr float kDeltaTime = 1.0f / 60.0f;

    // 破片を床へ落とす重力加速度
    constexpr float kGravity = 12.0f;

    // この時間を過ぎたら破片を徐々に小さくする
    constexpr float kShrinkStartTime = 1.2f;

    // この時間で死亡演出を終了する
    constexpr float kDeathEffectDuration = 2.2f;

    deathEffectTimer_ += kDeltaTime;

    // 縮小開始までは元の大きさを保つ
    float shrinkRatio = 1.0f;

    if (deathEffectTimer_ > kShrinkStartTime) {
        // 縮小開始から演出終了まで、1.0から0.0へ変化させる
        shrinkRatio = 1.0f -
            (deathEffectTimer_ - kShrinkStartTime) /
            (kDeathEffectDuration - kShrinkStartTime);

        // フレームずれで負の大きさにならないよう0.0で止める
        if (shrinkRatio < 0.0f) {
            shrinkRatio = 0.0f;
        }
    }

    for (DeathFragment& fragment : deathFragments_) {
        if (!fragment.object) {
            continue;
        }

        if (!fragment.isLanded) {
            // 重力でY方向の速度を下げる
            fragment.velocity.y -= kGravity * kDeltaTime;

            // 速度に従って位置を動かす
            fragment.position.x += fragment.velocity.x * kDeltaTime;
            fragment.position.y += fragment.velocity.y * kDeltaTime;
            fragment.position.z += fragment.velocity.z * kDeltaTime;

            // 飛んでいる間は回転させる
            fragment.rotation.x += fragment.angularVelocity.x * kDeltaTime;
            fragment.rotation.y += fragment.angularVelocity.y * kDeltaTime;
            fragment.rotation.z += fragment.angularVelocity.z * kDeltaTime;

            float groundY = 0.0f;

            // 破片のXZ位置にある床の高さを調べる
            if (GetGroundHeight(fragment.position.x, fragment.position.z, groundY)) {
                // 破片の中心が床より下へ入ったら接地させる
                const float fragmentBottomOffset = fragment.scale.y;

                if (fragment.position.y <= groundY + fragmentBottomOffset) {
                    fragment.position.y = groundY + fragmentBottomOffset;

                    // 落下速度が大きい間は小さく跳ね返す
                    if (fragment.velocity.y < -1.2f) {
                        fragment.velocity.y *= -0.22f;
                        fragment.velocity.x *= 0.55f;
                        fragment.velocity.z *= 0.55f;
                    } else {
                        // 勢いが弱くなったら床で停止させる
                        fragment.velocity = { 0.0f, 0.0f, 0.0f };
                        fragment.angularVelocity = { 0.0f, 0.0f, 0.0f };
                        fragment.isLanded = true;
                    }
                }
            }
        }

        // 当たり判定用の大きさは変えず、描画サイズだけを徐々に小さくする
        Vector3 drawScale = {
            fragment.scale.x * shrinkRatio,
            fragment.scale.y * shrinkRatio,
            fragment.scale.z * shrinkRatio
        };

        // 計算した位置、回転、縮小後の大きさを描画へ反映する
        fragment.object->SetScale(drawScale);
        fragment.object->SetRotate(fragment.rotation);
        fragment.object->SetTranslate(fragment.position);
        fragment.object->Update();
    }

    // 破片が完全に小さくなってから削除可能にする
    if (deathEffectTimer_ >= kDeathEffectDuration) {
        isReadyToRemove_ = true;
    }
}

bool Enemy::GetGroundHeight(float x, float z, float& groundY) const
{
    if (!floorColliders_) {
        return false;
    }

    bool foundGround = false;
    float highestGroundY = -FLT_MAX;

    for (const LevelColliderData& collider : *floorColliders_) {
        // BOX型の床だけを調べる
        if (!collider.hasCollider || collider.type != "BOX") {
            continue;
        }

        const float halfX = collider.size.x * 0.5f;
        const float halfY = collider.size.y * 0.5f;
        const float halfZ = collider.size.z * 0.5f;

        // 破片のXZ位置が床の範囲内か調べる
        if (x < collider.center.x - halfX ||
            x > collider.center.x + halfX ||
            z < collider.center.z - halfZ ||
            z > collider.center.z + halfZ) {
            continue;
        }

        // 複数の床が重なった場合は一番高い床を使用する
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
void Enemy::SetTargetPosition(const Vector3& targetPosition)
{
    // 追跡対象の位置を保存する
    targetPosition_ = targetPosition;
}

void Enemy::SetWaypoints(const std::vector<Vector3>& waypoints)
{
    // JSON から読んだウェイポイントを設定する
    waypointMover_.SetWaypoints(waypoints);
}

void Enemy::SetMap(const MapChipField* mapField, float tileSize)
{
    // CSV マップへの参照を保存する
    mapField_ = mapField;
    tileSize_ = tileSize;
}

bool Enemy::CheckTargetInSight() const
{
    // 敵からターゲットへのベクトルを求める
    Vector3 toTarget = {
        targetPosition_.x - position_.x,
        targetPosition_.y - (position_.y + sightHeight_),
        targetPosition_.z - position_.z
    };

    const float distance3D = std::sqrt(
        toTarget.x * toTarget.x +
        toTarget.y * toTarget.y +
        toTarget.z * toTarget.z
    );

    const float distanceXZ = std::sqrt(
        toTarget.x * toTarget.x +
        toTarget.z * toTarget.z
    );

    // 距離が短すぎるか射程外なら見えていない
    if (distance3D <= 0.0001f || distanceXZ > detectRange_) {
        return false;
    }

    // 敵の向きから前方ベクトルを求める
    Vector3 forward = {
        std::sin(rotation_.y),
        0.0f,
        std::cos(rotation_.y)
    };

    Vector3 directionXZSource = {
        toTarget.x,
        0.0f,
        toTarget.z
    };
    Vector3 directionXZ = Normalize(directionXZSource);
    float dot = DotVector3(forward, directionXZ);
    if (dot > 1.0f) {
        dot = 1.0f;
    }
    if (dot < -1.0f) {
        dot = -1.0f;
    }

    // 水平方向の視野角を超えたら見えていない
    const float horizontalAngle = std::acos(dot);
    if (horizontalAngle > sightHalfAngleRad_) {
        return false;
    }

    // 垂直方向の視野角も判定する
    const float verticalAngle = std::atan2(std::fabs(toTarget.y), distanceXZ);
    return verticalAngle <= sightVerticalHalfAngleRad_;
}

void Enemy::AppendVisionDebugLines(DebugLine3D& debugLine) const
{
    // 視野の始点を目線の高さに合わせる
    Vector3 origin = position_;
    origin.y += sightHeight_;

    // 左右の視野角を計算する
    const float centerYaw = rotation_.y;
    const float leftYaw = centerYaw - sightHalfAngleRad_;
    const float rightYaw = centerYaw + sightHalfAngleRad_;

    // 2D表示用にXZ平面上の左右端点だけを使う
    Vector3 leftEnd = {
        origin.x + std::sin(leftYaw) * detectRange_,
        origin.y,
        origin.z + std::cos(leftYaw) * detectRange_
    };
    Vector3 rightEnd = {
        origin.x + std::sin(rightYaw) * detectRange_,
        origin.y,
        origin.z + std::cos(rightYaw) * detectRange_
    };

    // 見えているときは緑、見えていないときは黄色にする
    Vector4 edgeColor = isTargetInSight_
        ? Vector4{ 0.1f, 0.8f, 0.1f, 1.0f }
        : Vector4{ 0.9f, 0.5f, 0.1f, 1.0f };

    // 中央線は描かず、左右の境界線と先端だけを描く
    debugLine.AddLine(origin, leftEnd, edgeColor);
    debugLine.AddLine(origin, rightEnd, edgeColor);
    debugLine.AddLine(leftEnd, rightEnd, edgeColor);
}
void Enemy::ResolveLeftCollisionWithMap(Vector3& pos)
{
    const float halfSize = tileSize_ * 0.5f;

    // 左へ動いていないときは判定しない
    if (pos.x >= prevPosition_.x) {
        return;
    }

    int mapWidth = mapField_->GetWidth();
    int mapHeight = mapField_->GetHeight();

    float enemyLeft = pos.x - halfSize;
    float enemyRight = pos.x + halfSize;
    float enemyBack = pos.z - halfSize;
    float enemyFront = pos.z + halfSize;
    float prevLeft = prevPosition_.x - halfSize;

    int tileX = static_cast<int>(std::floor(enemyLeft / tileSize_));
    if (tileX < 0 || tileX >= mapWidth) {
        return;
    }

    float bestBlockRight = -FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockBack = centerZ - halfSize;
        float blockFront = centerZ + halfSize;

        if (blockFront <= enemyBack || blockBack >= enemyFront) {
            continue;
        }

        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        if (prevLeft >= blockRight && enemyLeft <= blockRight && enemyRight > blockLeft) {
            if (blockRight > bestBlockRight) {
                bestBlockRight = blockRight;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.x = bestBlockRight + halfSize;
    }
}

void Enemy::ResolveRightCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    const float halfSize = tileSize_ * 0.5f;

    // 右へ動いていないときは判定しない
    if (pos.x <= prevPosition_.x) {
        return;
    }

    int mapWidth = mapField_->GetWidth();
    int mapHeight = mapField_->GetHeight();

    float enemyLeft = pos.x - halfSize;
    float enemyRight = pos.x + halfSize;
    float enemyBack = pos.z - halfSize;
    float enemyFront = pos.z + halfSize;
    float prevRight = prevPosition_.x + halfSize;

    int tileX = static_cast<int>(std::floor((enemyRight + halfSize) / tileSize_));
    if (tileX < 0 || tileX >= mapWidth) {
        return;
    }

    float bestBlockLeft = FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockBack = centerZ - halfSize;
        float blockFront = centerZ + halfSize;

        if (blockFront <= enemyBack || blockBack >= enemyFront) {
            continue;
        }

        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        if (prevRight <= blockLeft && enemyRight >= blockLeft && enemyLeft < blockRight) {
            if (blockLeft < bestBlockLeft) {
                bestBlockLeft = blockLeft;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.x = bestBlockLeft - halfSize;
    }
}

void Enemy::ResolveTopCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    // 前へ動いていないときは判定しない
    if (pos.z <= prevPosition_.z) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    float enemyFront = pos.z + halfSize;
    float prevFront = prevPosition_.z + halfSize;

    int tileX = static_cast<int>(std::floor(pos.x / tileSize_ + 0.5f));
    int mapWidth = mapField_->GetWidth();
    int mapHeight = mapField_->GetHeight();

    if (tileX < 0 || tileX >= mapWidth) {
        return;
    }

    float bestBlockBack = FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockBack = centerZ - halfSize;

        if (prevFront <= blockBack && enemyFront >= blockBack) {
            if (blockBack < bestBlockBack) {
                bestBlockBack = blockBack;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.z = bestBlockBack - halfSize;
    }
}

void Enemy::ResolveBottomCollisionWithMap(Vector3& pos)
{
    if (!mapField_) {
        return;
    }

    // 後ろへ動いていないときは判定しない
    if (pos.z >= prevPosition_.z) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    float enemyBack = pos.z - halfSize;
    float prevBack = prevPosition_.z - halfSize;

    int tileX = static_cast<int>(std::floor(pos.x / tileSize_ + 0.5f));
    int mapHeight = mapField_->GetHeight();

    float bestBlockFront = -FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockFront = centerZ + halfSize;

        if (prevBack >= blockFront && enemyBack <= blockFront) {
            if (blockFront > bestBlockFront) {
                bestBlockFront = blockFront;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.z = bestBlockFront + halfSize;
    }
}

void Enemy::ResolveGroundHeight(Vector3& pos)
{
    // 床コライダーがなければ何もしない
    if (!floorColliders_) {
        return;
    }

    bool foundGround = false;
    float bestGroundY = -FLT_MAX;

    // XZ が床の範囲外なら無視する
    for (const LevelColliderData& collider : *floorColliders_) {
        // BOX コライダーだけを対象にする
        if (!collider.hasCollider || collider.type != "BOX") {
            continue;
        }

        float halfX = collider.size.x * 0.5f;
        float halfY = collider.size.y * 0.5f;
        float halfZ = collider.size.z * 0.5f;

        float minX = collider.center.x - halfX;
        float maxX = collider.center.x + halfX;
        float minZ = collider.center.z - halfZ;
        float maxZ = collider.center.z + halfZ;

        // XZ 範囲に入っている床だけを調べる
        if (pos.x < minX || pos.x > maxX || pos.z < minZ || pos.z > maxZ) {
            continue;
        }

        // 床の上面 Y 座標を求める
        float groundY = collider.center.y + halfY;

        // 最も高い床を採用する
        if (!foundGround || groundY > bestGroundY) {
            bestGroundY = groundY;
            foundGround = true;
        }
    }

    // 接地できたら床の上に乗せる
    if (foundGround) {
        pos.y = bestGroundY + colliderRadius_;
    }
}

void Enemy::ResolveWallCollision(Vector3& pos)
{
    // 壁コライダーがなければ何もしない
    if (!wallColliders_) {
        return;
    }

    float halfSize = colliderRadius_;

    float enemyLeft = pos.x - halfSize;
    float enemyRight = pos.x + halfSize;
    float enemyBack = pos.z - halfSize;
    float enemyFront = pos.z + halfSize;

    float prevLeft = prevPosition_.x - halfSize;
    float prevRight = prevPosition_.x + halfSize;
    float prevBack = prevPosition_.z - halfSize;
    float prevFront = prevPosition_.z + halfSize;

    for (const LevelColliderData& collider : *wallColliders_) {
        if (!collider.hasCollider || collider.type != "BOX") {
            continue;
        }

        float halfX = collider.size.x * 0.5f;
        float halfZ = collider.size.z * 0.5f;

        float wallLeft = collider.center.x - halfX;
        float wallRight = collider.center.x + halfX;
        float wallBack = collider.center.z - halfZ;
        float wallFront = collider.center.z + halfZ;

        bool overlapX = (enemyRight > wallLeft && enemyLeft < wallRight);
        bool overlapZ = (enemyFront > wallBack && enemyBack < wallFront);
        if (!overlapX || !overlapZ) {
            continue;
        }

        if (prevRight <= wallLeft) {
            pos.x = wallLeft - halfSize;
        } else if (prevLeft >= wallRight) {
            pos.x = wallRight + halfSize;
        } else if (prevFront <= wallBack) {
            pos.z = wallBack - halfSize;
        } else if (prevBack >= wallFront) {
            pos.z = wallFront + halfSize;
        }
    }
}

void Enemy::UpdateRenderOnly()
{
    // デバッグカメラ用に transform だけ更新する
    Object3d* object = waypointMover_.GetObject3d();
    if (object && !isDead_) {
        object->SetScale(scale_);
        object->SetRotate(rotation_);
        object->SetTranslate(position_);
        object->Update();
    }
}
