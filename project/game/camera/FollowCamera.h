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

    // 指定した強さとフレーム数でカメラシェイクを開始する
    void StartShake(float power = 0.85f, int durationFrames = 24);

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

    // 追従補間だけを行った、シェイク前のカメラ位置
    Vector3 smoothedPosition_ = { 0.0f, 0.0f, 0.0f };

    // シェイクの最大振れ幅
    float shakePower_ = 0.0f;

    // シェイク全体のフレーム数
    int shakeDurationFrames_ = 0;

    // シェイクの残りフレーム数
    int shakeRemainingFrames_ = 0;

    // 初回だけ補間なしで合わせるためのフラグ
    bool isFirstUpdate_ = true;
};
