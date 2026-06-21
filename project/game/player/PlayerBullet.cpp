#include "PlayerBullet.h"

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
    ParticleSystem* particleSystem)
{
    // 初期位置を保存する
    position_ = position;

    // 初速度を保存する
    velocity_ = velocity;

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

    // 大きさを設定する
    // 弾本体を細長くして光の芯として見せる
    object_->SetScale({ 0.30f, 0.30f, 0.30f });

    // 細長くしたZ軸を弾の進行方向へ向ける
    float bulletAngle = std::atan2(velocity_.x, velocity_.z);
    object_->SetRotate({ 0.0f, bulletAngle, 0.0f });

    // ライトの影響を受けない明るい黄色にする
    object_->SetColor({ 1.0f, 0.92f, 0.34f, 0.65f });
    object_->GetMaterial()->lightingType = static_cast<int>(LightingType::None);

    // 位置を設定する
    object_->SetTranslate(position_);
}

void PlayerBullet::Update()
{
    if (!object_ || isDead_) {
        return;
    }

    // 弾を進める
    Vector3 previousPosition = position_;
    position_ = Add(position_, velocity_);

    // 点状の火花を重ねず、移動区間へ連続した発光軌跡だけを作る
    EmitTrail(previousPosition, position_);

    // Blender JSON の壁コライダーに入ったら弾を消す
    if (wallColliders_) {
        for (const LevelColliderData& collider : *wallColliders_) {
            // BOX collider だけ使う
            if (!collider.hasCollider || collider.type != "BOX") {
                continue;
            }

            float halfX = collider.size.x * 0.5f;
            float halfY = collider.size.y * 0.5f;
            float halfZ = collider.size.z * 0.5f;

            float minX = collider.center.x - halfX;
            float maxX = collider.center.x + halfX;
            float minY = collider.center.y - halfY;
            float maxY = collider.center.y + halfY;
            float minZ = collider.center.z - halfZ;
            float maxZ = collider.center.z + halfZ;

            // 弾の球コライダーと壁 AABB の最短距離を求める
            float closestX = std::clamp(position_.x, minX, maxX);
            float closestY = std::clamp(position_.y, minY, maxY);
            float closestZ = std::clamp(position_.z, minZ, maxZ);

            float diffX = position_.x - closestX;
            float diffY = position_.y - closestY;
            float diffZ = position_.z - closestZ;

            float distanceSq =
                diffX * diffX +
                diffY * diffY +
                diffZ * diffZ;

            // 壁に重なったら消す
            if (distanceSq <= colliderRadius_ * colliderRadius_) {
                if (particleSystem_) {
                    // Emit sparks in the direction opposite to the bullet travel.
                    Vector3 impactDirection = Normalize(Vector3{
                        -velocity_.x,
                        0.35f,
                        -velocity_.z
                    });

                    // Alternate yellow and orange colors for the impact sparks.
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

                    // Add a short-lived flash at the impact position.
                    particleSystem_->Emit(
                        position_,
                        { 0.55f, 0.55f, 0.55f },
                        { 0.0f, 0.0f, 0.0f },
                        { 1.0f, 0.90f, 0.50f, 0.85f },
                        0.08f);
                }

                isDead_ = true;
                return;
            }
        }
    }

    // 位置を更新する
    object_->SetTranslate(position_);
    object_->Update();

    // 生存時間を減らす
    lifeTime_--;

    // 時間切れで消す
    if (lifeTime_ <= 0) {
        isDead_ = true;
    }
}

void PlayerBullet::Draw()
{
    if (!object_ || isDead_) {
        return;
    }

    // 弾を描画する
    //object_->Draw();
}

void PlayerBullet::EmitTrail(const Vector3& start, const Vector3& end)
{
    if (!particleSystem_) {
        return;
    }

    // 丸い粒子の中心間隔を狭くし、加算合成の明るさを均一にする
    constexpr int kDivisionCount = 48;

    for (int index = 0; index < kDivisionCount; ++index) {
        // 区間の端ではなく各分割の中央へ置き、フレーム境界の粒子重複を防ぐ
        const float t =
            (static_cast<float>(index) + 0.5f) /
            static_cast<float>(kDivisionCount);

        Vector3 trailPosition = {
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
            start.z + (end.z - start.z) * t
        };

        // 後ろ側の赤橙色から先端側の黄色へ滑らかに変化させる
        const float trailGreen = 0.22f + 0.40f * t;
        const float trailBlue = 0.015f + 0.065f * t;

        // 粒子を少し大きくし、密に重ねて太めの一本線に見せる
        particleSystem_->Emit(
            trailPosition,
            { 0.30f, 0.30f, 0.30f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, trailGreen, trailBlue, 0.20f },
            0.11f);
    }

    // 軌跡の先端だけ少し明るくし、弾の現在位置を分かりやすくする
    particleSystem_->Emit(
        end,
        { 0.38f, 0.38f, 0.38f },
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.82f, 0.24f, 0.32f },
        0.045f);
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

void PlayerBullet::OnHit()
{
    // 当たった弾は消す
    isDead_ = true;
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

Vector3 PlayerBullet::GetMousePositionOnGround(Camera* camera, Input* input)
{
    if (!camera || !input) {
        return { 0.0f, 0.0f, 0.0f };
    }

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
    Vector3 worldNear = TransformPoint(nearPoint, inverseViewProjection);
    Vector3 worldFar = TransformPoint(farPoint, inverseViewProjection);

    // レイの向きを求める
    Vector3 rayVector = {
        worldFar.x - worldNear.x,
        worldFar.y - worldNear.y,
        worldFar.z - worldNear.z
    };

    Vector3 rayDirection = Normalize(rayVector);

    // 地面 y = 0 と交差する位置を求める
    float t = 0.0f;
    if (rayDirection.y != 0.0f) {
        t = -worldNear.y / rayDirection.y;
    }

    return {
        worldNear.x + rayDirection.x * t,
        0.0f,
        worldNear.z + rayDirection.z * t
    };
}
