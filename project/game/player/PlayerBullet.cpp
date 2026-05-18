#include "PlayerBullet.h"
#include "../camera/Camera.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/winapp/WinApp.h"

void PlayerBullet::Initialize(
    Object3dCommon* object3dCommon,
    const Vector3& position,
    const Vector3& velocity)
{
    // 初期位置を保存する
    position_ = position;

    // 初速度を保存する
    velocity_ = velocity;

    // 弾オブジェクトを作る
    object_ = std::make_unique<Object3d>();

    // 3D描画の共通設定を渡す
    object_->Initialize(object3dCommon);

    // 弾モデルを設定する
    object_->SetModel("bullet/bullet.obj");

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

    // 位置を反映する
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

Vector3 PlayerBullet::CalcDirectionToMouseGround(
    const Vector3& startPosition,
    Camera* camera,
    Input* input)
{
    // マウスが指している地面の位置を取る
    Vector3 targetPosition = GetMousePositionOnGround(camera, input);

    // 発射位置から狙い位置への方向を作る
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

    // ビュープロジェクション行列の逆行列を作る
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

    // レイの向きを作る
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
