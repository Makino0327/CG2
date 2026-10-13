#include "PlayerBullet.h"
#include "ShootingBloom.h"
#include "BulletAim.h"
#include "BulletFlight.h"

#include <algorithm>
#include <cmath>

#include "../camera/Camera.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/winapp/WinApp.h"
#include "../../engine/particle/Particle.h"

void PlayerBullet::Initialize(
    Object3dCommon* object3dCommon,
    const Vector3& position,
    const Vector3& velocity,
    const std::vector<LevelColliderData>* wallColliders,
    ParticleSystem* particleSystem,
    std::optional<float> flightHeight)
{
    // 初期位置を保存する
    position_ = previousPosition_ = movementEnd_ = position;
    wallHitTime_ = 1.0f;
    isDead_ = wallHitPending_ = effectsPending_ = false;
    lifeTime_ = 120;

    // 初速度を保存する
    velocity_ = velocity;
    flightHeight_ = flightHeight;

    // Blender JSON の壁コライダー一覧を保存する
    wallColliders_ = wallColliders;

    // 全プレイヤー弾で共有する軌跡用パーティクルを保存する
    particleSystem_ = particleSystem;

    // 弾オブジェクトを作る
    object_ = std::make_unique<Object3d>();

    // 3D オブジェクトを初期化する
    object_->Initialize(object3dCommon);

    // 弾モデルを設定する
    object_->SetModel("bullet/bullet.obj");

    // 弾本体は小さく細長い形にし、発射炎より目立たないようにする。
    object_->SetScale({ 0.045f, 0.045f, 0.12f });

    // 細長くしたZ軸を進行方向へ向け、銃口から下向きに撃つ場合も弾と軌跡をそろえる。
    float bulletAngle = std::atan2(velocity_.x, velocity_.z);
    const float horizontalSpeed = std::sqrt(velocity_.x*velocity_.x + velocity_.z*velocity_.z);
    const float bulletPitch = -std::atan2(velocity_.y, horizontalSpeed);
    object_->SetRotate({ bulletPitch, bulletAngle, 0.0f });

    // 弾本体の金属色を少し明るくし、細い発光の芯と合わせて見やすくする。
    object_->SetColor({ 0.85f, 0.68f, 0.35f, 1.0f });
    object_->SetLightingType(LightingType::HalfLambert);

    // 位置を設定する
    object_->SetTranslate(position_);
}

void PlayerBullet::Update(bool deferEffects)
{
    if (!object_ || isDead_) { return; }

    // 移動前の位置を保存し、弾が通った区間全体を判定する。
    previousPosition_ = position_;
    AdvanceBulletFlight(position_, velocity_, flightHeight_);
    wallHitPending_ = false;
    effectsPending_ = true;

    // 一覧の順序に関係なく、移動区間で最初に触れる壁を探す。
    movementEnd_ = position_;
    float nearestTime = 1.0f;
    if (wallColliders_) {
        for (const LevelColliderData& collider : *wallColliders_) {
            if (!collider.hasCollider || collider.type != "BOX") { continue; }
            const Vector3 minimum{
                collider.center.x-collider.size.x*0.5f,
                collider.center.y-collider.size.y*0.5f,
                collider.center.z-collider.size.z*0.5f };
            const Vector3 maximum{
                collider.center.x+collider.size.x*0.5f,
                collider.center.y+collider.size.y*0.5f,
                collider.center.z+collider.size.z*0.5f };
            float hitTime = 0.0f;
            if (Collision::SweepSphereAabb(previousPosition_, position_, colliderRadius_,
                minimum, maximum, hitTime) && hitTime <= nearestTime) {
                nearestTime = hitTime;
                wallHitPending_ = true;
            }
        }
    }
    // 壁より向こう側の敵は判定せず、手前の敵への命中を先に調べられるようにする。
    wallHitTime_ = nearestTime;
    if (wallHitPending_) { position_ = Lerp(previousPosition_, movementEnd_, wallHitTime_); }
    object_->SetTranslate(position_);
    object_->Update();
    if (!deferEffects) { FinishUpdate(); }
}

void PlayerBullet::FinishUpdate()
{
    if (!effectsPending_) { return; }
    effectsPending_ = false;
    // 敵への命中で位置が短縮された場合も、軌跡をその位置までに収める。
    EmitTrail(previousPosition_, position_);
    if (!isDead_ && wallHitPending_) {
        EmitWallImpact();
        isDead_ = true;
    }
    // 最終フレームの当たり判定と軌跡を処理してから、寿命で消す。
    if (--lifeTime_ <= 0) { isDead_ = true; }
}

void PlayerBullet::EmitWallImpact()
{
    if (!particleSystem_) { return; }
    // 弾の進行方向と反対側へ着弾の火花を飛ばす。
    Vector3 impactDirection = Normalize(Vector3{
        -velocity_.x,
        0.35f,
        -velocity_.z
    });

    // 火花の色を黄色とオレンジで交互に変える。
    for (int index = 0; index < 10; ++index) {
        const float sideOffset =
            (static_cast<float>(index % 5) - 2.0f) * 0.35f;

        particleSystem_->Emit(
            position_,
            { 0.12f, 0.12f, 0.12f },
            {
                impactDirection.x * 2.5f + sideOffset,
                impactDirection.y * 2.5f + index * 0.08f,
                impactDirection.z * 2.5f - sideOffset
            },
            index % 2 == 0
                ? Vector4{ 1.0f, 0.75f, 0.20f, 0.90f }
                : Vector4{ 1.0f, 0.30f, 0.04f, 0.80f },
            0.18f);
    }

    // 命中した位置へ短い閃光を出す。
    particleSystem_->Emit(
        position_,
        { 0.55f, 0.55f, 0.55f },
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.90f, 0.50f, 0.85f },
        0.08f);
}

void PlayerBullet::Draw()
{
    if (!object_ || isDead_) {
        return;
    }

    // 前と同じく、弾の見た目は先端の発光粒子と軌跡で表現する。
}

void PlayerBullet::EmitTrail(const Vector3& start, const Vector3& end)
{
    if (!particleSystem_ || !particleSystem_->IsSegmentVisible(start, end)) {
        return;
    }

    // 距離に応じて粒子数を決め、短い区間へ大量の粒子を出して枠を使い切らないようにする。
    // 白い芯は全ての点へ出し、最も速い散弾でも点の間隔を一定以下に保つ。
    // 前の発光軌跡の色と寿命を保ち、全ての粒子を80%の大きさにする。
    constexpr float kVisualScale = 0.80f;
    constexpr float kMaxPointSpacing = 0.06f * kVisualScale;
    const float dx = end.x-start.x;
    const float dy = end.y-start.y;
    const float dz = end.z-start.z;
    const float distance = std::sqrt(dx*dx+dy*dy+dz*dz);
    const int divisionCount = (std::max)(1, static_cast<int>(std::ceil(distance/kMaxPointSpacing)));

    for (int index = 0; index < divisionCount; ++index) {
        // 区間の端ではなく各分割の中央へ置き、フレーム境界の粒子重複を防ぐ
        const float t =
            (static_cast<float>(index) + 0.5f) /
            static_cast<float>(divisionCount);

        Vector3 trailPosition = {
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
            start.z + (end.z - start.z) * t
        };

        // 軌跡の周囲に大きく薄いオレンジ色の光を重ね、光のにじみを作る。
        // 3点に1回だけ出し、連射や散弾でも光が強くなりすぎないようにする。
        if (index % 3 == 0) {
            particleSystem_->Emit(
                trailPosition,
                { 0.60f * kVisualScale, 0.60f * kVisualScale, 0.60f * kVisualScale },
                { 0.0f, 0.0f, 0.0f },
                { 1.0f, 0.45f, 0.08f, 0.08f },
                0.06f, ShootingBloom::bulletTrailStrength);
        }

        // 後ろ側の赤橙色から先端側の黄色へ滑らかに変化させる
        const float trailGreen = 0.22f + 0.40f * t;
        const float trailBlue = 0.015f + 0.065f * t;

        // 粒子を少し大きくし、密に重ねて太めの一本線に見せる
        particleSystem_->Emit(
            trailPosition,
            { 0.30f * kVisualScale, 0.30f * kVisualScale, 0.30f * kVisualScale },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, trailGreen, trailBlue, 0.32f },
            0.11f, ShootingBloom::bulletTrailStrength);

        // 細く短い白黄色の芯を重ね、周囲の薄い光との明るさの差を付ける。
        particleSystem_->Emit(
            trailPosition,
            { 0.10f * kVisualScale, 0.10f * kVisualScale, 0.10f * kVisualScale },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.95f, 0.72f, 0.32f },
            0.045f, ShootingBloom::bulletTrailStrength);
    }

    // 軌跡の先端は白黄色へ寄せ、弾の現在位置が最も明るく見えるようにする。
    particleSystem_->Emit(
        end,
        { 0.38f * kVisualScale, 0.38f * kVisualScale, 0.38f * kVisualScale },
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.95f, 0.65f, 0.45f },
        0.045f, ShootingBloom::bulletTrailStrength);
}

void PlayerBullet::EmitSparks()
{
    if (!particleSystem_) {
        return;
    }

    // 粒子数が増えすぎないよう2フレームごとに火花を出す
    sparkFrame_++;
    if ((sparkFrame_ % 2) != 0) {
        return;
    }

    Vector3 direction = Normalize(velocity_);
    Vector3 sideDirection = { -direction.z, 0.0f, direction.x };

    // 発生方向が機械的に見えないよう左右の強さを交互に変える
    float variation = ((sparkFrame_ / 2) % 2 == 0) ? 1.0f : 0.65f;

    Vector3 leftPosition = {
        position_.x + sideDirection.x * 0.16f,
        position_.y + 0.04f,
        position_.z + sideDirection.z * 0.16f
    };
    Vector3 rightPosition = {
        position_.x - sideDirection.x * 0.13f,
        position_.y - 0.03f,
        position_.z - sideDirection.z * 0.13f
    };

    // 左側へ剥がれる金色の火花
    particleSystem_->Emit(
        leftPosition,
        { 0.11f, 0.11f, 0.11f },
        {
            sideDirection.x * 2.8f * variation - direction.x * 0.8f,
            0.8f,
            sideDirection.z * 2.8f * variation - direction.z * 0.8f
        },
        { 1.0f, 0.72f, 0.18f, 0.85f },
        0.16f);

    // 右側へ剥がれる濃い橙色の火花
    particleSystem_->Emit(
        rightPosition,
        { 0.085f, 0.085f, 0.085f },
        {
            -sideDirection.x * 2.2f - direction.x * 1.0f,
            -0.35f,
            -sideDirection.z * 2.2f - direction.z * 1.0f
        },
        { 1.0f, 0.38f, 0.06f, 0.78f },
        0.20f);

    // ときどき上方向へ小さな白黄色の火花を追加する
    if ((sparkFrame_ % 4) == 0) {
        particleSystem_->Emit(
            position_,
            { 0.07f, 0.07f, 0.07f },
            {
                sideDirection.x * 0.8f,
                1.8f,
                sideDirection.z * 0.8f
            },
            { 1.0f, 0.9f, 0.45f, 0.9f },
            0.13f);
    }
}

SphereCollider PlayerBullet::GetCollider() const
{
    // 現在位置と半径から球コライダーを返す
    return { position_, colliderRadius_ };
}

bool PlayerBullet::CheckHit(const SphereCollider& target, float& hitTime) const
{
    // 壁と同じフレーム時刻で判定し、壁より後の命中は除外する。
    return Collision::SweepSphere(previousPosition_, movementEnd_, colliderRadius_, target, hitTime)
        && hitTime <= wallHitTime_;
}

void PlayerBullet::OnHit(float hitTime)
{
    // 移動途中の命中位置へ戻し、軌跡が敵を通り抜けないようにする。
    position_ = Lerp(previousPosition_, movementEnd_, std::clamp(hitTime, 0.0f, wallHitTime_));
    if (object_) { object_->SetTranslate(position_); }
    isDead_ = true;
}

bool PlayerBullet::CheckHit(const BoxCollider& target, float& hitTime) const
{
    // 止まっている箱は、移動前と移動後の中心を同じ位置として判定する。
    return CheckHit(target, target.center, hitTime);
}

bool PlayerBullet::CheckHit(const BoxCollider& target, const Vector3& previousCenter, float& hitTime) const
{
    // 弾と敵の移動を同じ時間軸で調べ、壁より手前での命中だけを採用する。
    return Collision::SweepMovingSphereBox(previousPosition_, movementEnd_, colliderRadius_,
        target, previousCenter, hitTime) && hitTime <= wallHitTime_;
}

Vector3 PlayerBullet::CalcDirectionToMouseGround(
    const Vector3& startPosition,
    Camera* camera,
    Input* input)
{
    // マウスが指している地面の位置を求める
    Vector3 targetPosition = GetMousePositionOnGround(camera, input);

    // 発射位置から目標位置への方向を求める
    Vector3 direction = {
        targetPosition.x - startPosition.x,
        0.0f,
        targetPosition.z - startPosition.z
    };

    return Normalize(direction);
}

Vector3 PlayerBullet::CalcDirectionToMouseAtHeight(
    const Vector3& startPosition,
    const Vector3& playerPosition,
    Camera* camera,
    Input* input,
    float targetHeight)
{
    if (!camera || !input) { return { 0.0f, 0.0f, 0.0f }; }

    // マウス位置を狙う高さへ投影し、床への投影による近距離のずれを防ぐ。
    const Vector3 targetPosition = GetMousePositionOnPlane(camera, input, targetHeight);
    return CalculateBulletAimDirection(startPosition, playerPosition, targetPosition, targetHeight);
}

Vector3 PlayerBullet::GetMousePositionOnGround(Camera* camera, Input* input)
{
    // 移動方向やグレネードは、これまで通り床の位置を使う。
    return GetMousePositionOnPlane(camera, input, 0.0f);
}

bool PlayerBullet::GetMouseRay(Camera* camera, Input* input, Vector3& start, Vector3& end)
{
    if (!camera || !input) { return false; }
    // マウス座標を取得する
    Vector2 mousePosition = input->GetMousePosition();

    // 画面座標を -1 から 1 の範囲に変換する
    float ndcX = (mousePosition.x / static_cast<float>(WinApp::kClientWidth)) * 2.0f - 1.0f;
    float ndcY = -((mousePosition.y / static_cast<float>(WinApp::kClientHeight)) * 2.0f - 1.0f);

    // ビュープロジェクション行列の逆行列を求める
    Matrix4x4 inverseViewProjection = Inverse(camera->GetViewProjectionMatrix());

    Vector4 nearPoint = { ndcX, ndcY, 0.0f, 1.0f };
    Vector4 farPoint = { ndcX, ndcY, 1.0f, 1.0f };

    auto TransformPoint = [](const Vector4& point, const Matrix4x4& matrix) -> Vector3 {
        float w =
            point.x * matrix.m[0][3] +
            point.y * matrix.m[1][3] +
            point.z * matrix.m[2][3] +
            point.w * matrix.m[3][3];

        Vector3 result{};
        result.x =
            (point.x * matrix.m[0][0] +
                point.y * matrix.m[1][0] +
                point.z * matrix.m[2][0] +
                point.w * matrix.m[3][0]) / w;

        result.y =
            (point.x * matrix.m[0][1] +
                point.y * matrix.m[1][1] +
                point.z * matrix.m[2][1] +
                point.w * matrix.m[3][1]) / w;

        result.z =
            (point.x * matrix.m[0][2] +
                point.y * matrix.m[1][2] +
                point.z * matrix.m[2][2] +
                point.w * matrix.m[3][2]) / w;

        return result;
    };

    // 画面上の点をワールド座標へ戻す
    start = TransformPoint(nearPoint, inverseViewProjection);
    end = TransformPoint(farPoint, inverseViewProjection);

    // 不正な投影座標を当たり判定へ渡さない。
    return std::isfinite(start.x) && std::isfinite(start.y) && std::isfinite(start.z)
        && std::isfinite(end.x) && std::isfinite(end.y) && std::isfinite(end.z);
}

Vector3 PlayerBullet::GetMousePositionOnPlane(Camera* camera, Input* input, float height)
{
    Vector3 start{}, end{};
    if (!GetMouseRay(camera, input, start, end)) { return { 0, height, 0 }; }
    const Vector3 delta{ end.x-start.x, end.y-start.y, end.z-start.z };
    // 指定した高さの平面へ、マウスから伸ばした線を交差させる。
    if (std::abs(delta.y) <= 0.00001f) { return { start.x, height, start.z }; }
    const float t = (height-start.y)/delta.y;
    return { start.x+delta.x*t, height, start.z+delta.z*t };
}
