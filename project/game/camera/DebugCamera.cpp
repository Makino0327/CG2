#include "DebugCamera.h"
#include "Camera.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include <cmath>

void DebugCamera::Update(Camera* camera, Input* input, OffscreenRenderer* offscreenRenderer, bool isDebugMode)
{
    if (!camera || !input || !offscreenRenderer) {
        return;
    }

    Transform& transform = camera->GetTransform();

    // デバッグ開始時のカメラ姿勢を保存する
    if (isDebugMode && !wasDebugMode_) {
        savedTranslate_ = transform.translate;
        savedRotate_ = transform.rotate;
    }

    // デバッグ終了時に元のカメラ姿勢へ戻す
    if (!isDebugMode && wasDebugMode_) {
        transform.translate = savedTranslate_;
        transform.rotate = savedRotate_;
    }

    wasDebugMode_ = isDebugMode;

    // デバッグモードが無効なら入力を受け付けない
    if (!isDebugMode) {
        return;
    }

    // Game View 上で右クリック中だけ視点操作を受け付ける
    const bool canControlCamera =
        offscreenRenderer->IsGameViewHovered() &&
        input->PushMouseRight();

    if (!canControlCamera) {
        return;
    }

    Vector2 mouseDelta = offscreenRenderer->GetGameViewMouseDelta();

    // 横移動で Yaw を回す
    transform.rotate.y += mouseDelta.x * rotateSpeed_;

    // 縦移動で Pitch を回す
    transform.rotate.x += mouseDelta.y * rotateSpeed_;

    // Pitch の回りすぎを防ぐ
    if (transform.rotate.x > 1.2f) {
        transform.rotate.x = 1.2f;
    }
    if (transform.rotate.x < -1.2f) {
        transform.rotate.x = -1.2f;
    }

    float pitch = transform.rotate.x;
    float yaw = transform.rotate.y;

    // 視線方向ベースの前方ベクトルを作る
    Vector3 forward = {
        std::sin(yaw) * std::cos(pitch),
        -std::sin(pitch),
        std::cos(yaw) * std::cos(pitch)
    };
    forward = Normalize(forward);

    // 左右移動は水平面上だけにする
    Vector3 right = {
        std::cos(yaw),
        0.0f,
        -std::sin(yaw)
    };
    right = Normalize(right);

    // 前後移動
    if (input->PushKey(DIK_W)) {
        transform.translate.x += forward.x * moveSpeed_;
        transform.translate.y += forward.y * moveSpeed_;
        transform.translate.z += forward.z * moveSpeed_;
    }
    if (input->PushKey(DIK_S)) {
        transform.translate.x -= forward.x * moveSpeed_;
        transform.translate.y -= forward.y * moveSpeed_;
        transform.translate.z -= forward.z * moveSpeed_;
    }

    // 左右移動
    if (input->PushKey(DIK_A)) {
        transform.translate.x -= right.x * moveSpeed_;
        transform.translate.z -= right.z * moveSpeed_;
    }
    if (input->PushKey(DIK_D)) {
        transform.translate.x += right.x * moveSpeed_;
        transform.translate.z += right.z * moveSpeed_;
    }

    // 上昇
    if (input->PushKey(DIK_SPACE)) {
        transform.translate.y += moveSpeed_;
    }

    // 下降
    if (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT)) {
        transform.translate.y -= moveSpeed_;
    }
}
