#pragma once
#include <string>
#include <vector>
#include "../../math/Math.h"
#include "../../2d/sprite/Sprite.h"

struct MaterialData
{
    std::string textureFilePath;
    uint32_t textureIndex = 0;
};

struct MeshData {
    // メッシュ名
    std::string name;

    // 頂点配列
    std::vector<VertexData> vertices;

    // index 配列
    std::vector<uint32_t> indices;
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
};
