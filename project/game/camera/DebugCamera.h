#pragma once

#include "../../engine/math/Math.h"

class Camera;
class Input;
class OffscreenRenderer;

class DebugCamera
{
public:
    // デバッグ用のカメラ操作を更新する
    void Update(Camera* camera, Input* input, OffscreenRenderer* offscreenRenderer, bool isDebugMode);

private:
    // カメラの移動速度
    float moveSpeed_ = 0.15f;

    // カメラの回転速度
    float rotateSpeed_ = 0.003f;

    // デバッグ開始前のカメラ位置
    Vector3 savedTranslate_ = { 0.0f, 0.0f, 0.0f };

    // デバッグ開始前のカメラ回転
    Vector3 savedRotate_ = { 0.0f, 0.0f, 0.0f };

    // 前フレームでデバッグ中だったか
    bool wasDebugMode_ = false;

};
