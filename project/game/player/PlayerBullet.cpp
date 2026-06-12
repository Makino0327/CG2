#include "PlayerBullet.h"

#include <algorithm>

#include "../camera/Camera.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/winapp/WinApp.h"

void PlayerBullet::Initialize(
    Object3dCommon* object3dCommon,
    const Vector3& position,
    const Vector3& velocity,
    const std::vector<LevelColliderData>* wallColliders)
{
    // 初期位置を保存する
    position_ = position;

    // 初速度を保存する
    velocity_ = velocity;

    // Blender JSON の壁コライダー一覧を保存する
    wallColliders_ = wallColliders;

    // 弾オブジェクトを作る
    object_ = std::make_unique<Object3d>();

    // 3D オブジェクトを初期化する
    object_->Initialize(object3dCommon);

    // 弾モデルを設定する
    object_->SetModel("bullet/bullet.obj");

    // 弾だけ環境マップ反射を使う
    object_->SetEnvironmentTexture("Resources/skybox.dds");

    // 弾の反射の強さを設定する
    object_->SetEnvironmentCoefficient(0.2f);

    // 大きさを設定する
    object_->SetScale(scale_);

    // 位置を設定する
    object_->SetTranslate(position_);
}

void PlayerBullet::Update()
{
    if (!object_ || isDead_) {
        return;
    }

    // 弾を進める
    position_ = Add(position_, velocity_);

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
    object_->Draw();
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
