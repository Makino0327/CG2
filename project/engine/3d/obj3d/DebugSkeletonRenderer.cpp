#include "DebugSkeletonRenderer.h"
#include <cmath>

void DebugSkeletonRenderer::Initialize(DirectXCommon* dxCommon, Line3DCommon* line3dCommon)
{
    // 線描画クラスを初期化する
    debugLine_.Initialize(dxCommon, line3dCommon);
}

void DebugSkeletonRenderer::Draw()
{
    // 中で保持している線分を描画する
    debugLine_.Draw();
}

Vector3 DebugSkeletonRenderer::ExtractTranslation(const Matrix4x4& matrix) const
{
    Vector3 result;

    // 行列の平行移動成分を取り出す
    result.x = matrix.m[3][0];
    result.y = matrix.m[3][1];
    result.z = matrix.m[3][2];

    return result;
}

void DebugSkeletonRenderer::AddWireSphere(const Vector3& center, float radius, const Vector4& color)
{
    // 円の分割数
    const int kSubdivision = 16;

    // 円周率
    const float kPi = 3.1415926535f;

    // 3つの平面に円を描いてワイヤー球に見せる
    for (int axis = 0; axis < 3; ++axis) {
        for (int i = 0; i < kSubdivision; ++i) {
            float theta0 = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(kSubdivision);
            float theta1 = (2.0f * kPi * static_cast<float>(i + 1)) / static_cast<float>(kSubdivision);

            Vector3 p0 = center;
            Vector3 p1 = center;

            if (axis == 0) {
                // YZ平面の円
                p0.y += std::cos(theta0) * radius;
                p0.z += std::sin(theta0) * radius;
                p1.y += std::cos(theta1) * radius;
                p1.z += std::sin(theta1) * radius;
            } else if (axis == 1) {
                // XZ平面の円
                p0.x += std::cos(theta0) * radius;
                p0.z += std::sin(theta0) * radius;
                p1.x += std::cos(theta1) * radius;
                p1.z += std::sin(theta1) * radius;
            } else {
                // XY平面の円
                p0.x += std::cos(theta0) * radius;
                p0.y += std::sin(theta0) * radius;
                p1.x += std::cos(theta1) * radius;
                p1.y += std::sin(theta1) * radius;
            }

            // 円弧を短い線分として追加する
            debugLine_.AddLine(p0, p1, color);
        }
    }
}

void DebugSkeletonRenderer::Build(
    const Skeleton& skeleton,
    const Matrix4x4& worldMatrix,
    const Matrix4x4& viewProjectionMatrix)
{
    debugLine_.Reset();

    const Vector4 jointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const Vector4 boneColor = { 1.0f, 1.0f, 1.0f, 1.0f };
   

    for (const Joint& joint : skeleton.joints) {
        Matrix4x4 jointWorldMatrix = Multiply(joint.skeletonSpaceMatrix, worldMatrix);
        Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);

        AddWireSphere(jointPosition, jointRadius_, jointColor);


        if (joint.parent) {
            const Joint& parentJoint = skeleton.joints[*joint.parent];
            Matrix4x4 parentWorldMatrix = Multiply(parentJoint.skeletonSpaceMatrix, worldMatrix);
            Vector3 parentPosition = ExtractTranslation(parentWorldMatrix);

            debugLine_.AddLine(parentPosition, jointPosition, boneColor);
        }
    }

    debugLine_.SetWVP(MakeIdentity4x4(), viewProjectionMatrix);
    debugLine_.Upload();
}

void DebugSkeletonRenderer::SetJointRadius(float radius)
{
    // 0 以下にならないようにする
    if (radius > 0.0f) {
        jointRadius_ = radius;
    }
}
