#pragma once
#include "Math.h"
class Camera
{
private:
    Transform transform_;
    Matrix4x4 worldMatrix_{};
    Matrix4x4 viewMatrix_{};
    Matrix4x4 viewProjectionMatrix_;

    // === プロジェクション行列関連 ===
    Matrix4x4 projectionMatrix_;
    float fovY_;
    float aspectRatio_;
    float nearClip_;
    float farClip_;

    Matrix4x4 billboardMatrix_;

public:
    Camera();

    // 更新
    void Update();


    // === Transform 関連 ===
    void SetRotate(const Vector3& rotate) {
        transform_.rotate = rotate;
    }

    void SetTranslate(const Vector3& translate) {
        transform_.translate = translate;
    }

    // === Projection 関連 ===
    void SetFovY(float fovY) {
        fovY_ = fovY;
    }

    void SetAspectRatio(float aspect) {
        aspectRatio_ = aspect;
    }

    void SetNearClip(float nearClip) {
        nearClip_ = nearClip;
    }

    void SetFarClip(float farClip) {
        farClip_ = farClip;
    }

    Transform& GetTransform() { return transform_; }

    // === 行列の getter ===
    const Matrix4x4& GetWorldMatrix() const {
        return worldMatrix_;
    }

    const Matrix4x4& GetViewMatrix() const {
        return viewMatrix_;
    }

    const Matrix4x4& GetProjectionMatrix() const {
        return projectionMatrix_;
    }

    const Matrix4x4& GetViewProjectionMatrix() const {
        return viewProjectionMatrix_;
    }

    // === transform材料の getter ===
    Vector3 GetRotate() const {
        return transform_.rotate;
    }

    Vector3 GetTranslate() const {
        return transform_.translate;
    }

    const Matrix4x4& GetBillboardMatrix() const { return billboardMatrix_; }
};

