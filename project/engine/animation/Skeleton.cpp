#include "Skeleton.h"
#include <cassert>

void BlendSkeletonPose(Skeleton& skeleton, const std::vector<QuaternionTransform>& sourcePose, float progress)
{
    assert(sourcePose.size() == skeleton.joints.size());
    const float t = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    // Smoothstepで始点と終点の速度を落とし、急な姿勢切り替えをなくす。
    const float eased = t * t * (3.0f - 2.0f * t);
    for (size_t index = 0; index < skeleton.joints.size(); ++index) {
        auto& target = skeleton.joints[index].transform;
        const auto& source = sourcePose[index];
        target.translate = Lerp(source.translate, target.translate, eased);
        target.rotate = Slerp(source.rotate, target.rotate, eased);
        target.scale = Lerp(source.scale, target.scale, eased);
    }
}

namespace {
    // 親の回転に子の回転を合成するハミルトン積。
    Quaternion ComposeRotation(const Quaternion& parent, const Quaternion& local)
    {
        return Normalize({
            parent.w * local.x + parent.x * local.w + parent.y * local.z - parent.z * local.y,
            parent.w * local.y - parent.x * local.z + parent.y * local.w + parent.z * local.x,
            parent.w * local.z + parent.x * local.y - parent.y * local.x + parent.z * local.w,
            parent.w * local.w - parent.x * local.x - parent.y * local.y - parent.z * local.z
        });
    }
}

Quaternion GetJointModelRotation(const Skeleton& skeleton, int32_t jointIndex)
{
    Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::optional<int32_t> current = jointIndex;
    while (current) {
        const Joint& joint = skeleton.joints[*current];
        rotation = ComposeRotation(joint.transform.rotate, rotation);
        current = joint.parent;
    }
    return rotation;
}

void CorrectJointParentRotation(Skeleton& skeleton, int32_t jointIndex, const Quaternion& referenceParentRotation)
{
    Joint& joint = skeleton.joints[jointIndex];
    const Quaternion currentParent = joint.parent
        ? GetJointModelRotation(skeleton, *joint.parent)
        : Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
    const Quaternion inverseParent{ -currentParent.x, -currentParent.y, -currentParent.z, currentParent.w };
    // 構え・反動のモデル空間での向きを保ち、現在の腰に対する回転へ変換する。
    joint.transform.rotate = ComposeRotation(inverseParent,
        ComposeRotation(referenceParentRotation, joint.transform.rotate));
}

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints)
{
    Joint joint; // // 今から追加する Joint を作る
    joint.name = node.name; // // Node 名をそのまま Joint 名にする
    joint.transform = node.transform; // // TRS を引き継ぐ
    joint.localMatrix = node.localMatrix; // // localMatrix も引き継ぐ
    joint.skeletonSpaceMatrix = MakeIdentity4x4(); // // 後で UpdateSkeleton で計算する
    joint.index = static_cast<int32_t>(joints.size()); // // 現在の配列サイズを自分の index にする
    joint.parent = parent; // // 親 index を保存する

    joints.push_back(joint); // // まず自分を joints に追加する

    // // 子 Node を順番に Joint 化して children に登録する
    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, joint.index, joints); // // 子 Joint を再帰生成する
        joints[joint.index].children.push_back(childIndex); // // 自分の children に子 index を追加する
    }

    return joint.index; // // 自分の index を返す
}

Skeleton CreateSkeleton(const Node& rootNode)
{
    Skeleton skeleton; // // 作成する Skeleton 本体

    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints); // // rootNode から再帰的に Joint を作る

    // // Joint 名から index を引けるように辞書を作る
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    return skeleton; // // 完成した Skeleton を返す
}

void UpdateSkeleton(Skeleton& skeleton)
{
    // // 親 Joint は必ず自分より若い index に入っている前提で順番に更新する
    for (Joint& joint : skeleton.joints) {
        // // 現在の transform から localMatrix を作り直す
        joint.localMatrix = MakeAffineMatrix(
            joint.transform.scale,
            joint.transform.rotate,
            joint.transform.translate
        );

        if (joint.parent) {
            // // 親がいるなら親の skeletonSpaceMatrix を掛けて自分の姿勢を作る
            joint.skeletonSpaceMatrix = Multiply(
                joint.localMatrix,
                skeleton.joints[*joint.parent].skeletonSpaceMatrix
            );
        } else {
            // // root Joint は親がいないので local がそのまま skeleton 空間になる
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime)
{
    for (Joint& joint : skeleton.joints) {
        // Joint 名に対応する animation を探す
        auto it = animation.nodeAnimations.find(joint.name);
        if (it == animation.nodeAnimations.end()) {
            continue;
        }

        const NodeAnimation& nodeAnimation = it->second;

        // translate のキーフレームがあれば現在時刻の値を適用する
        if (!nodeAnimation.translate.keyframes.empty()) {
            joint.transform.translate =
                CalculateValue(nodeAnimation.translate.keyframes, animationTime);
        }

        // rotate のキーフレームがあれば現在時刻の値を適用する
        if (!nodeAnimation.rotate.keyframes.empty()) {
            joint.transform.rotate =
                CalculateValue(nodeAnimation.rotate.keyframes, animationTime);
        }

        // scale のキーフレームがあれば現在時刻の値を適用する
        if (!nodeAnimation.scale.keyframes.empty()) {
            joint.transform.scale =
                CalculateValue(nodeAnimation.scale.keyframes, animationTime);
        }
    }
}
