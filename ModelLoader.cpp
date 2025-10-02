#include "ModelLoader.h"
#include <fstream>
#include <sstream>
#include <cassert>

ModelData ModelLoader::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;
	/// 一時バッファ
	// 頂点
	std::vector<Vector4> positions;
	// 法線
	std::vector<Vector3> normals;
	// UV座標
	std::vector<Vector2> texcoords;

	std::string line;

	// OBJファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	bool flipY = (filename != "plane.obj");
	assert(file.is_open());

	// 現在のメッシュ情報(oやgで入れ替わる)
	MeshData currentMesh;
	currentMesh.name = "Default"; // 名前がない場合

	// ファイルを一行ずつ読み込む
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			// 頂点座標
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			// テクスチャ座標
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			// 法線ベクトル
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		} else if (identifier == "f") {
			// 面（ポリゴン）
			VertexData triangle[3];
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3]; // v/vt/vnのインデックス
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				// インデックスの始まりなのでー1する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				// 左手座標系への変換
				if (flipY) {
					position.x *= -1.0f;
					normal.x *= -1.0f;

					// z軸回転で姿勢補正(180度)
					float rad = 3.141592f;
					float x = position.x;
					float z = position.z;
					position.x = x * cos(rad) - z * sin(rad);
					position.z = x * sin(rad) + z * cos(rad);
				}

				triangle[faceVertex] = { position, texcoord, normal };
			}

			// 逆順で三角形を追加(面の向きを反転しないように）
			currentMesh.vertices.push_back(triangle[2]);
			currentMesh.vertices.push_back(triangle[1]);
			currentMesh.vertices.push_back(triangle[0]);
		} else if (identifier == "o" || identifier == "g") {
			// オブジェクト名またはグループ名
			if (!currentMesh.vertices.empty()) {
				modelData.meshes.push_back(currentMesh);
				currentMesh = MeshData();
			}

			std::string meshName;
			s >> meshName;
			currentMesh.name = meshName;
		}
	}

	// 最後のメッシュも忘れずに追加
	if (!currentMesh.vertices.empty()) {
		modelData.meshes.push_back(currentMesh);
	}

	return modelData;
}