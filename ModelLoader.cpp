#include "ModelLoader.h"
#include <fstream>
#include <sstream>
#include <cassert>

ModelData ModelLoader::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	bool flipY = (filename != "plane.obj");
	assert(file.is_open());

	MeshData currentMesh;
	currentMesh.name = "Default";

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		} else if (identifier == "f") {
			VertexData triangle[3];
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				if (flipY) {
					position.x *= -1.0f;
					normal.x *= -1.0f;

					float rad = 3.141592f;
					float x = position.x;
					float z = position.z;
					position.x = x * cos(rad) - z * sin(rad);
					position.z = x * sin(rad) + z * cos(rad);
				}

				triangle[faceVertex] = { position, texcoord, normal };
			}

			currentMesh.vertices.push_back(triangle[2]);
			currentMesh.vertices.push_back(triangle[1]);
			currentMesh.vertices.push_back(triangle[0]);
		} else if (identifier == "o" || identifier == "g") {
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