#include "DebugSkeletonRenderer.h"

#include <cmath>

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

namespace {

bool ContainsText(const std::string& text, const char* keyword)
{
    return text.find(keyword) != std::string::npos;
}

} // namespace

void DebugSkeletonRenderer::Initialize(DirectXCommon* dxCommon, Line3DCommon* line3dCommon)
{
    // 線描画クラスを初期化する
    debugLine_.Initialize(dxCommon, line3dCommon);
}

void DebugSkeletonRenderer::Draw()
{
    // Buildで作った骨線と関節点を描画する
    debugLine_.Draw();
}

void DebugSkeletonRenderer::DrawJointLabels()
{
#ifdef USE_IMGUI
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
    if (!drawList) {
        return;
    }

    for (const JointLabel& label : jointLabels_) {
        Vector2 screenPosition{};
        if (!ProjectToScreen(label.worldPosition, screenPosition)) {
            continue;
        }

        // Joint位置のすぐ近くに名前を置く
        ImVec2 textPosition(screenPosition.x + 4.0f, screenPosition.y - 6.0f);

        // 黒い影を先に描いて、白文字を読みやすくする
        drawList->AddText(
            ImVec2(textPosition.x + 1.0f, textPosition.y + 1.0f),
            IM_COL32(0, 0, 0, 230),
            label.name.c_str());
        drawList->AddText(
            textPosition,
            IM_COL32(255, 255, 255, 255),
            label.name.c_str());
    }
#endif
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
    // 点として見せたいので分割数は少なめにする
    const int kSubdivision = 8;
    const float kPi = 3.1415926535f;

    // 3つの平面に小さい円を描いて関節点に見せる
    for (int axis = 0; axis < 3; ++axis) {
        for (int i = 0; i < kSubdivision; ++i) {
            float theta0 = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(kSubdivision);
            float theta1 = (2.0f * kPi * static_cast<float>(i + 1)) / static_cast<float>(kSubdivision);

            Vector3 p0 = center;
            Vector3 p1 = center;

            if (axis == 0) {
                // YZ平面の円を作る
                p0.y += std::cos(theta0) * radius;
                p0.z += std::sin(theta0) * radius;
                p1.y += std::cos(theta1) * radius;
                p1.z += std::sin(theta1) * radius;
            } else if (axis == 1) {
                // XZ平面の円を作る
                p0.x += std::cos(theta0) * radius;
                p0.z += std::sin(theta0) * radius;
                p1.x += std::cos(theta1) * radius;
                p1.z += std::sin(theta1) * radius;
            } else {
                // XY平面の円を作る
                p0.x += std::cos(theta0) * radius;
                p0.y += std::sin(theta0) * radius;
                p1.x += std::cos(theta1) * radius;
                p1.y += std::sin(theta1) * radius;
            }

            debugLine_.AddLine(p0, p1, color);
        }
    }
}

bool DebugSkeletonRenderer::ShouldDrawJointLabel(const std::string& jointName) const
{
    // Endボーンや指は数が多く読みにくいので表示しない
    if (ContainsText(jointName, "_End") ||
        ContainsText(jointName, "Thumb") ||
        ContainsText(jointName, "Index") ||
        ContainsText(jointName, "Middle") ||
        ContainsText(jointName, "Ring") ||
        ContainsText(jointName, "Pinky")) {
        return false;
    }

    // 画像の例に近い主要Jointだけ表示する
    return ContainsText(jointName, "Hips") ||
        ContainsText(jointName, "Spine") ||
        ContainsText(jointName, "Neck") ||
        ContainsText(jointName, "Head") ||
        ContainsText(jointName, "Shoulder") ||
        ContainsText(jointName, "Arm") ||
        ContainsText(jointName, "ForeArm") ||
        ContainsText(jointName, "Hand") ||
        ContainsText(jointName, "UpLeg") ||
        ContainsText(jointName, "Leg") ||
        ContainsText(jointName, "Foot");
}

std::string DebugSkeletonRenderer::MakeDisplayName(const std::string& jointName) const
{
    // mixamorig: のような接頭辞を消して短く表示する
    const size_t colonPos = jointName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < jointName.size()) {
        return jointName.substr(colonPos + 1);
    }

    return jointName;
}

bool DebugSkeletonRenderer::ProjectToScreen(const Vector3& worldPosition, Vector2& screenPosition) const
{
#ifdef USE_IMGUI
    float clipX =
        worldPosition.x * viewProjectionMatrix_.m[0][0] +
        worldPosition.y * viewProjectionMatrix_.m[1][0] +
        worldPosition.z * viewProjectionMatrix_.m[2][0] +
        viewProjectionMatrix_.m[3][0];
    float clipY =
        worldPosition.x * viewProjectionMatrix_.m[0][1] +
        worldPosition.y * viewProjectionMatrix_.m[1][1] +
        worldPosition.z * viewProjectionMatrix_.m[2][1] +
        viewProjectionMatrix_.m[3][1];
    float clipW =
        worldPosition.x * viewProjectionMatrix_.m[0][3] +
        worldPosition.y * viewProjectionMatrix_.m[1][3] +
        worldPosition.z * viewProjectionMatrix_.m[2][3] +
        viewProjectionMatrix_.m[3][3];

    if (clipW <= 0.001f) {
        return false;
    }

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f) {
        return false;
    }

    if (screenSize_.x <= 0.0f || screenSize_.y <= 0.0f) {
        return false;
    }

    // Game Viewの画像範囲内に投影して、骨の位置に名前が乗るようにする
    screenPosition.x = screenTopLeft_.x + (ndcX * 0.5f + 0.5f) * screenSize_.x;
    screenPosition.y = screenTopLeft_.y + (-ndcY * 0.5f + 0.5f) * screenSize_.y;

    return true;
#else
    screenPosition = {};
    return false;
#endif
}

void DebugSkeletonRenderer::Build(
    const Skeleton& skeleton,
    const Matrix4x4& worldMatrix,
    const Matrix4x4& viewProjectionMatrix)
{
    debugLine_.Reset();
    jointLabels_.clear();
    viewProjectionMatrix_ = viewProjectionMatrix;

    const Vector4 jointColor = { 0.95f, 0.95f, 0.95f, 1.0f };
    const Vector4 boneColor = { 0.72f, 0.72f, 0.72f, 1.0f };

    for (const Joint& joint : skeleton.joints) {
        Matrix4x4 jointWorldMatrix = Multiply(joint.skeletonSpaceMatrix, worldMatrix);
        Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);

        // 関節位置を小さい白いワイヤー球で表示する
        AddWireSphere(jointPosition, jointRadius_, jointColor);

        if (ShouldDrawJointLabel(joint.name)) {
            // 主要Jointだけ名前表示用に保存する
            jointLabels_.push_back({ MakeDisplayName(joint.name), jointPosition });
        }

        if (joint.parent) {
            const Joint& parentJoint = skeleton.joints[*joint.parent];
            Matrix4x4 parentWorldMatrix = Multiply(parentJoint.skeletonSpaceMatrix, worldMatrix);
            Vector3 parentPosition = ExtractTranslation(parentWorldMatrix);

            // 親子Jointをグレーの線でつなげて骨として見せる
            debugLine_.AddLine(parentPosition, jointPosition, boneColor);
        }
    }

    debugLine_.SetWVP(MakeIdentity4x4(), viewProjectionMatrix);
    debugLine_.Upload();
}

void DebugSkeletonRenderer::SetJointRadius(float radius)
{
    // 0以下にならないようにする
    if (radius > 0.0f) {
        jointRadius_ = radius;
    }
}
void DebugSkeletonRenderer::SetScreenRect(const Vector2& topLeft, const Vector2& size)
{
    // ImGuiのGame View画像がある範囲を保存する
    screenTopLeft_ = topLeft;
    screenSize_ = size;
}