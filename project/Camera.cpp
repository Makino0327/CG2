#include "Camera.h"
#include "WinApp.h"

void Camera::Update()
{
    // ① transform から world（SRT、左手座標系）
    worldMatrix_ = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate
    );

    // ② view = world の逆
    viewMatrix_ = Inverse(worldMatrix_);

    // ③ projection（fovY, aspect, near, far）
    projectionMatrix_ = MakePerspectiveFovMatrix(
        fovY_, aspectRatio_, nearClip_, farClip_
    );

    // ④ ★ VP 合成（今回のスライド部分）
    viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

    // ⑤ ★ビルボード用：回転だけ取り出した行列
    billboardMatrix_ = worldMatrix_;
    billboardMatrix_.m[3][0] = 0.0f;
    billboardMatrix_.m[3][1] = 0.0f;
    billboardMatrix_.m[3][2] = 0.0f;
}

Camera::Camera()
    : transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} })
    , fovY_(0.45f)
    , aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
    , nearClip_(0.1f)
    , farClip_(100.0f)
    , worldMatrix_(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
    , viewMatrix_(Inverse(worldMatrix_))
    , projectionMatrix_(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
    , viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_))
{
}
