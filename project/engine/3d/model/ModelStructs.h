#pragma once
#include <map>
#include <string>
#include <vector>
#include "../../math/Math.h"
#include "../../2d/sprite/Sprite.h"

struct MaterialData
{
    std::string textureFilePath;
    uint32_t textureIndex = 0;
    Vector4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct MeshData {
    // メッシュ名
    std::string name;

    // 頂点配列
    std::vector<VertexData> vertices;

    // index 配列
    std::vector<uint32_t> indices;

    // glTFのprimitiveごとに違う色やテクスチャを保持する
    MaterialData material;
};

struct VertexWeightData {
    // この Joint が頂点に与える重み
    float weight;

    // 影響を受ける頂点番号
    uint32_t vertexIndex;
};

struct JointWeightData {
    // この Joint の inverse bind pose matrix
    Matrix4x4 inverseBindPoseMatrix;

    // この Joint が影響を与える頂点一覧
    std::vector<VertexWeightData> vertexWeights;
};

struct Node {
    // glTF から読んだ TRS
    QuaternionTransform transform;

    // transform から作り直したローカル行列
    Matrix4x4 localMatrix;

    // node 名
    std::string name;

    // 子 node 一覧
    std::vector<Node> children;
};

struct ModelData {
    MaterialData material;
    std::vector<MeshData> meshes;

    // モデルの node 階層の根
    Node rootNode;

    // Joint 名ごとの Skinning 情報
    std::map<std::string, JointWeightData> skinClusterData;
};
