#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "../math/Math.h"
#include "../3d/model/ModelStructs.h"
#include "Animation.h"


struct Joint {
    QuaternionTransform transform;         // // Joint の TRS 情報
    Matrix4x4 localMatrix;                 // // Joint のローカル行列
    Matrix4x4 skeletonSpaceMatrix;         // // Skeleton 空間での行列
    std::string name;                      // // Joint 名
    std::vector<int32_t> children;         // // 子 Joint の index 一覧
    int32_t index;                         // // 自分自身の index
    std::optional<int32_t> parent;         // // 親 Joint の index。無ければ nullopt
};

struct Skeleton {
    int32_t root;                          // // root Joint の index
    std::map<std::string, int32_t> jointMap; // // Joint 名から index を引く辞書
    std::vector<Joint> joints;             // // Skeleton に所属する Joint 一覧
};

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
Skeleton CreateSkeleton(const Node& rootNode);
void UpdateSkeleton(Skeleton& skeleton);
// Animation を Skeleton に適用する
void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

// 親をたどってモデル空間での骨の回転を求める。
Quaternion GetJointModelRotation(const Skeleton& skeleton, int32_t jointIndex);
// 別モーションから移した上半身に、移動側の腰のひねりが重複しないよう補正する。
void CorrectJointParentRotation(Skeleton& skeleton, int32_t jointIndex, const Quaternion& referenceParentRotation);

// 切り替え直前の姿勢から現在の姿勢へ、滑らかな加減速でつなぐ。
void BlendSkeletonPose(Skeleton& skeleton, const std::vector<QuaternionTransform>& sourcePose, float progress);
