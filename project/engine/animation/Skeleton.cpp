#include "Skeleton.h"
#include <cassert>

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
