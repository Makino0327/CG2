#include "FollowCamera.h"
#include "Camera.h"

void FollowCamera::Initialize(Camera* camera)
{
    // 使用するカメラを覚える
    camera_ = camera;

    // シーン開始直後はターゲット位置へ即座に合わせる
    isFirstUpdate_ = true;
}

void FollowCamera::SetTarget(const Vector3& targetPosition)
{
    // 追従対象の位置を保存する
    targetPosition_ = targetPosition;
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
        camera_->SetTranslate(targetCameraPosition);
        camera_->SetRotate(rotate_);
        camera_->Update();
        isFirstUpdate_ = false;
        return;
    }

    // 現在位置から目標位置へなめらかに近づける
    Vector3 currentCameraPosition = camera_->GetTranslate();
    Vector3 nextCameraPosition = Lerp(currentCameraPosition, targetCameraPosition, easeRate_);

    // カメラの位置と角度を反映する
    camera_->SetTranslate(nextCameraPosition);
    camera_->SetRotate(rotate_);
    camera_->Update();
}
