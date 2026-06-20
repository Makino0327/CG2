#include "FollowCamera.h"
#include "Camera.h"

#include <cmath>

void FollowCamera::Initialize(Camera* camera)
{
    // 使用するカメラを覚える
    camera_ = camera;

    // シーン開始直後はターゲット位置へ即座に合わせる
    isFirstUpdate_ = true;

    // 前のシーンで使ったシェイク状態を残さない
    shakePower_ = 0.0f;
    shakeDurationFrames_ = 0;
    shakeRemainingFrames_ = 0;
}

void FollowCamera::SetTarget(const Vector3& targetPosition)
{
    // 追従対象の位置を保存する
    targetPosition_ = targetPosition;
}

void FollowCamera::StartShake(float power, int durationFrames)
{
    if (power <= 0.0f || durationFrames <= 0) {
        return;
    }

    // 新しい爆発が来たら最大振れ幅と時間を最初から設定する
    shakePower_ = power;
    shakeDurationFrames_ = durationFrames;
    shakeRemainingFrames_ = durationFrames;
}

void FollowCamera::Update()
{
    if (!camera_) {
        return;
    }

    // プレイヤー位置にオフセットを足して目標カメラ位置を作る
    Vector3 targetCameraPosition = {
        targetPosition_.x + offset_.x,
        targetPosition_.y + offset_.y,
        targetPosition_.z + offset_.z
    };

    // 初回だけは補間せずに直接合わせる
    if (isFirstUpdate_) {
        smoothedPosition_ = targetCameraPosition;
        isFirstUpdate_ = false;
    } else {
        // シェイクを含まない位置だけを、目標位置へなめらかに近づける
        smoothedPosition_ = Lerp(smoothedPosition_, targetCameraPosition, easeRate_);
    }

    Vector3 finalCameraPosition = smoothedPosition_;

    if (shakeRemainingFrames_ > 0 && shakeDurationFrames_ > 0) {
        // 終了へ近づくほど振れ幅を小さくする
        const float remainingRatio =
            static_cast<float>(shakeRemainingFrames_) /
            static_cast<float>(shakeDurationFrames_);
        const float elapsedFrames = static_cast<float>(
            shakeDurationFrames_ - shakeRemainingFrames_);
        const float currentPower = shakePower_ * remainingRatio;

        // 軸ごとに違う周期を使い、機械的な往復に見えない揺れを作る
        finalCameraPosition.x += std::sin(elapsedFrames * 2.37f) * currentPower;
        finalCameraPosition.y += std::cos(elapsedFrames * 3.11f) * currentPower * 0.45f;
        finalCameraPosition.z += std::sin(elapsedFrames * 1.73f) * currentPower * 0.65f;

        shakeRemainingFrames_--;
    }

    // 追従位置へシェイクを加えた最終位置をカメラへ反映する
    camera_->SetTranslate(finalCameraPosition);
    camera_->SetRotate(rotate_);
    camera_->Update();
}
