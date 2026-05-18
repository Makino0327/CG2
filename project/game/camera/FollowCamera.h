#pragma once

#include "../../engine/math/Math.h"

class Camera;

class FollowCamera
{
public:
    // カメラを初期化する
    void Initialize(Camera* camera);

    // ターゲット座標に向かってカメラを追従させる
    void Update();

    // 追従対象のワールド座標を設定する
    void SetTarget(const Vector3& targetPosition);

private:
    // 実際に動かすカメラ
    Camera* camera_ = nullptr;

    // 追従対象の座標
    Vector3 targetPosition_ = { 0.0f, 0.0f, 0.0f };

    // プレイヤーから見たカメラのずらし量
    Vector3 offset_ = { 0.0f, 60.0f, -7.0f };

    // 少し上から見るための回転
    Vector3 rotate_ = { 1.5f, 0.0f, 0.0f };

    // 追従のなめらかさ
    float easeRate_ = 0.08f;

    // 初回だけ補間なしで合わせるためのフラグ
    bool isFirstUpdate_ = true;
};
