#pragma once
#include <array>
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/model/ModelManager.h"

// タイトルと本編で、同じ銃口の頂点と描画行列を使って発射位置を求める。
inline Vector3 GetPlayerMuzzlePosition(const Object3d& object) {
    // Cube.204_2にある、銃口の穴と銃身の奥側の穴を囲む10頂点。
    constexpr std::array<uint32_t,10> kMuzzleVertices{ 30,38,46,54,62,70,78,86,94,102 };
    constexpr std::array<uint32_t,10> kBarrelRearVertices{ 110,113,116,119,122,125,128,131,134,137 };
    constexpr float kMuzzleClearance = 0.03f;

    const ModelData& modelData = ModelManager::GetInstance()->FindModel("player_modular/Swat.gltf")->GetModelData();
    const MeshData* gunMesh = nullptr;
    uint32_t vertexOffset = 0;
    for (const MeshData& mesh : modelData.meshes) {
        // 同じPistolメッシュ内の指の部分ではなく、銃口を持つ描画部分を選ぶ。
        if (mesh.name == "Cube.204_2") { gunMesh = &mesh; break; }
        // 骨の重みが使う頂点番号は、全メッシュを連結した配列の番号。
        vertexOffset += static_cast<uint32_t>(mesh.vertices.size());
    }
    assert(gunMesh && gunMesh->vertices.size() > kBarrelRearVertices.back());
    const SkinCluster& skinCluster = object.GetSkinCluster();
    // 足元補正も含め、実際の描画に渡したワールド行列をそのまま使う。
    const Matrix4x4& worldMatrix = object.GetWorldMatrix();
    const auto transformPoint = [](const Vector3& value, const Matrix4x4& matrix) {
        return Vector3{
            value.x*matrix.m[0][0]+value.y*matrix.m[1][0]+value.z*matrix.m[2][0]+matrix.m[3][0],
            value.x*matrix.m[0][1]+value.y*matrix.m[1][1]+value.z*matrix.m[2][1]+matrix.m[3][1],
            value.x*matrix.m[0][2]+value.y*matrix.m[1][2]+value.z*matrix.m[2][2]+matrix.m[3][2]
        };
    };
    const auto getRingCenter = [&](const auto& vertexIndices) {
        Vector3 center{};
        for (uint32_t index : vertexIndices) {
            const Vector4& position = gunMesh->vertices[index].position;
            const Vector3 bindPosition{ position.x,position.y,position.z };
            const VertexInfluence& influence = skinCluster.mappedInfluence[vertexOffset+index];
            Vector3 skinnedPosition{};
            // GPUと同じ骨番号・重みで頂点を変形し、移動や射撃反動にも追従する。
            for (uint32_t slot = 0; slot < kNumMaxInfluence; ++slot) {
                const float weight = influence.weights[slot];
                if (weight <= 0.0f) { continue; }
                const Vector3 point = transformPoint(bindPosition,
                    skinCluster.mappedPalette[influence.jointIndices[slot]].skeletonSpaceMatrix);
                skinnedPosition.x += point.x*weight;
                skinnedPosition.y += point.y*weight;
                skinnedPosition.z += point.z*weight;
            }
            const Vector3 worldPosition = transformPoint(skinnedPosition, worldMatrix);
            center.x += worldPosition.x;
            center.y += worldPosition.y;
            center.z += worldPosition.z;
        }
        const float count = static_cast<float>(vertexIndices.size());
        return Vector3{ center.x/count,center.y/count,center.z/count };
    };
    const Vector3 muzzle = getRingCenter(kMuzzleVertices);
    const Vector3 rear = getRingCenter(kBarrelRearVertices);
    const Vector3 forward = Normalize(Vector3{ muzzle.x-rear.x,muzzle.y-rear.y,muzzle.z-rear.z });
    // 弾や発射炎が銃に埋まらないよう、銃口から少しだけ外へ出す。
    return { muzzle.x+forward.x*kMuzzleClearance,
        muzzle.y+forward.y*kMuzzleClearance, muzzle.z+forward.z*kMuzzleClearance };
}
