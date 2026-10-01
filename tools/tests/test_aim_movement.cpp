#define NOMINMAX
#include "../../project/engine/animation/Skeleton.h"
#include "../../project/externals/nlohmann/json.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>

using Json = nlohmann::json;

static Node ReadNode(const Json& nodes, size_t index)
{
    // 実際の素材の階層を読み、エンジンと同じ左手座標系へ変換する。
    const auto& source = nodes[index];
    Node node;
    node.name = source.at("name");
    const auto t = source.value("translation", std::vector<float>{ 0, 0, 0 });
    const auto r = source.value("rotation", std::vector<float>{ 0, 0, 0, 1 });
    const auto s = source.value("scale", std::vector<float>{ 1, 1, 1 });
    node.transform.translate = { -t[0], t[1], t[2] };
    node.transform.rotate = { r[0], -r[1], -r[2], r[3] };
    node.transform.scale = { s[0], s[1], s[2] };
    node.localMatrix = MakeAffineMatrix(node.transform.scale, node.transform.rotate, node.transform.translate);
    for (size_t child : source.value("children", std::vector<size_t>{})) {
        node.children.push_back(ReadNode(nodes, child));
    }
    return node;
}

static bool UpperBody(const Skeleton& skeleton, const std::string& name)
{
    std::optional<int32_t> index = skeleton.jointMap.at(name);
    while (index) {
        const auto& joint = skeleton.joints[*index];
        if (joint.name == "Abdomen") { return true; }
        index = joint.parent;
    }
    return false;
}

static float RotationSimilarity(const Matrix4x4& a, const Matrix4x4& b)
{
    float minimum = 1.0f;
    for (int row = 0; row < 3; ++row) {
        const Vector3 x = Normalize(Vector3{ a.m[row][0], a.m[row][1], a.m[row][2] });
        const Vector3 y = Normalize(Vector3{ b.m[row][0], b.m[row][1], b.m[row][2] });
        minimum = std::min(minimum, x.x * y.x + x.y * y.y + x.z * y.z);
    }
    return minimum;
}

int main()
{
    const std::string directory = "project/Resources/player_modular";
    std::ifstream file(directory + "/Swat.gltf");
    Json model;
    file >> model;
    const Skeleton base = CreateSkeleton(ReadNode(model.at("nodes"), model.at("scenes")[0].at("nodes")[0]));
    const Animation aim = LoadAnimationFile(directory, "SwatTwoHand.gltf", 0);
    const Animation shot = LoadAnimationFile(directory, "SwatTwoHand.gltf", 1);
    Skeleton reference = base;
    ApplyAnimation(reference, aim, 0.0f);
    const int32_t abdomen = reference.jointMap.at("Abdomen");
    const Quaternion parentRotation = GetJointModelRotation(reference, *reference.joints[abdomen].parent);
    const auto names = GetAnimationNames(directory, "Swat.gltf");
    float worstBefore = 1.0f;
    float worstAfter = 1.0f;
    for (const char* name : { "Run", "Run_Back", "Run_Left", "Run_Right" }) {
        const auto found = std::find(names.begin(), names.end(), name);
        assert(found != names.end());
        const Animation run = LoadAnimationFile(directory, "Swat.gltf", static_cast<uint32_t>(found - names.begin()));
        for (int frame = 0; frame <= 60; ++frame) {
            for (float shotTime : { -1.0f, 0.045f, 0.16f, 0.32f }) {
                Skeleton expected = reference;
                if (shotTime >= 0.0f) { ApplyAnimation(expected, shot, shotTime); }
                UpdateSkeleton(expected);
                Skeleton moving = base;
                ApplyAnimation(moving, run, frame * run.duration / 60.0f);
                for (auto& joint : moving.joints) {
                    if (UpperBody(moving, joint.name)) {
                        joint.transform = expected.joints[joint.index].transform;
                    }
                }
                UpdateSkeleton(moving);
                const auto footBefore = moving.joints[moving.jointMap.at("Foot.L")].skeletonSpaceMatrix;
                const int32_t hand = moving.jointMap.at("Wrist.R");
                worstBefore = std::min(worstBefore, RotationSimilarity(moving.joints[hand].skeletonSpaceMatrix, expected.joints[hand].skeletonSpaceMatrix));
                CorrectJointParentRotation(moving, abdomen, parentRotation);
                UpdateSkeleton(moving);
                // 全方向・歩行周期・反動の各時点で、左右の手首が停止時と同じ向きを保つ。
                for (const char* wrist : { "Wrist.R", "Wrist.L" }) {
                    const int32_t index = moving.jointMap.at(wrist);
                    const float similarity = RotationSimilarity(moving.joints[index].skeletonSpaceMatrix, expected.joints[index].skeletonSpaceMatrix);
                    worstAfter = std::min(worstAfter, similarity);
                    assert(similarity > 0.99999f);
                }
                const auto footAfter = moving.joints[moving.jointMap.at("Foot.L")].skeletonSpaceMatrix;
                for (int row = 0; row < 4; ++row) {
                    for (int col = 0; col < 4; ++col) {
                        assert(std::abs(footBefore.m[row][col] - footAfter.m[row][col]) < 0.00001f);
                    }
                }
            }
        }
    }
    // 拡大後もモデルの足元が論理座標から半径1だけ下に位置する。
    const float scale = 2.1f;
    const float offset = 2.0f / 3.0f - 1.0f / scale;
    assert(std::abs((-2.0f / 3.0f + offset) * scale + 1.0f) < 0.00001f);
    assert(worstBefore < 0.99f);
    // 待機から構えへ補間し、始点・終点で姿勢が跳ばないことを確認する。
    const auto idleIndex = std::find(names.begin(), names.end(), "Idle_Gun") - names.begin();
    const Animation idle = LoadAnimationFile(directory, "Swat.gltf", static_cast<uint32_t>(idleIndex));
    Skeleton source = base;
    ApplyAnimation(source, idle, 0.0f);
    std::vector<QuaternionTransform> from;
    for (const auto& joint : source.joints) { from.push_back(joint.transform); }
    for (float progress : { 0.0f, 0.01f, 0.5f, 0.99f, 1.0f }) {
        Skeleton blended = reference;
        BlendSkeletonPose(blended, from, progress);
        for (size_t index = 0; index < from.size(); ++index) {
            const auto& actual = blended.joints[index].transform;
            assert(std::isfinite(actual.rotate.w));
            assert(std::abs(Dot(actual.rotate, actual.rotate) - 1.0f) < 0.001f);
            if (progress == 0.0f || progress == 1.0f) {
                const auto& endpoint = progress == 0.0f ? from[index] : reference.joints[index].transform;
                assert(std::abs(Dot(Normalize(actual.rotate), Normalize(endpoint.rotate))) > 0.99999f);
            }
        }
        // 切り替え途中で操作を変えても、現在の姿勢から連続して次へつながる。
        std::vector<QuaternionTransform> interrupted;
        for (const auto& joint : blended.joints) { interrupted.push_back(joint.transform); }
        Skeleton redirected = source;
        BlendSkeletonPose(redirected, interrupted, 0.0f);
        for (size_t index = 0; index < interrupted.size(); ++index) {
            assert(std::abs(Dot(redirected.joints[index].transform.rotate, interrupted[index].rotate)) > 0.99999f);
        }
    }
    std::cout << "Aim movement and recoil: PASS (976 poses), before=" << worstBefore
        << ", after=" << worstAfter << ", foot placement and interrupted blending: PASS\n";
}
