#include "Model.h"

#include <cstring>
#include <filesystem>
#include <regex>
#include "../obj3d/Object3d.h"


namespace {

	struct GltfAccessor {
		uint32_t bufferView = 0;
		uint32_t byteOffset = 0;
		uint32_t componentType = 0;
		uint32_t count = 0;
		std::string type;
	};

	struct GltfBufferView {
		uint32_t buffer = 0;
		uint32_t byteOffset = 0;
		uint32_t byteLength = 0;
	};

	std::string ReadTextFile(const std::string& filePath)
	{
		std::ifstream file(filePath);
		assert(file.is_open());

		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	std::vector<uint8_t> ReadBinaryFile(const std::string& filePath)
	{
		std::ifstream file(filePath, std::ios::binary);
		assert(file.is_open());

		file.seekg(0, std::ios::end);
		size_t size = static_cast<size_t>(file.tellg());
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> data(size);
		file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
		return data;
	}

	size_t FindMatchingBracket(const std::string& text, size_t openIndex, char openChar, char closeChar)
	{
		int depth = 0;
		for (size_t i = openIndex; i < text.size(); ++i) {
			if (text[i] == openChar) {
				++depth;
			} else if (text[i] == closeChar) {
				--depth;
				if (depth == 0) {
					return i;
				}
			}
		}

		assert(false);
		return std::string::npos;
	}
	size_t FindTopLevelKey(const std::string& json, const std::string& key)
	{
		std::string target = "\"" + key + "\"";

		int objectDepth = 0;
		bool inString = false;

		for (size_t i = 0; i < json.size(); ++i) {
			char c = json[i];

			// 文字列の外でだけ深さを数える
			if (!inString) {
				if (c == '{') {
					++objectDepth;
				} else if (c == '}') {
					--objectDepth;
				}
			}

			// 最上位オブジェクト直下にあるキーだけ探す
			if (!inString && objectDepth == 1) {
				if (json.compare(i, target.size(), target) == 0) {
					return i;
				}
			}

			// エスケープされていない " で文字列状態を切り替える
			if (c == '"' && (i == 0 || json[i - 1] != '\\')) {
				inString = !inString;
			}
		}

		return std::string::npos;
	}


	std::string ExtractArrayBlock(const std::string& json, const std::string& key)
	{
		size_t keyPos = FindTopLevelKey(json, key);
		assert(keyPos != std::string::npos);

		size_t arrayBegin = json.find('[', keyPos);
		assert(arrayBegin != std::string::npos);

		size_t arrayEnd = FindMatchingBracket(json, arrayBegin, '[', ']');
		return json.substr(arrayBegin, arrayEnd - arrayBegin + 1);
	}


	std::vector<std::string> SplitTopLevelObjects(const std::string& arrayBlock)
	{
		std::vector<std::string> objects;
		int depth = 0;
		size_t objectBegin = std::string::npos;

		for (size_t i = 0; i < arrayBlock.size(); ++i) {
			if (arrayBlock[i] == '{') {
				if (depth == 0) {
					objectBegin = i;
				}
				++depth;
			} else if (arrayBlock[i] == '}') {
				--depth;
				if (depth == 0 && objectBegin != std::string::npos) {
					objects.push_back(arrayBlock.substr(objectBegin, i - objectBegin + 1));
					objectBegin = std::string::npos;
				}
			}
		}

		return objects;
	}

	std::string FindStringValue(const std::string& objectText, const std::string& key)
	{
		std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
		std::smatch match;
		bool found = std::regex_search(objectText, match, pattern);
		assert(found);
		return match[1].str();
	}

	uint32_t FindUIntValue(const std::string& objectText, const std::string& key)
	{
		std::regex pattern("\"" + key + "\"\\s*:\\s*(\\d+)");
		std::smatch match;
		bool found = std::regex_search(objectText, match, pattern);
		assert(found);
		return static_cast<uint32_t>(std::stoul(match[1].str()));
	}

	uint32_t FindUIntValueOrDefault(const std::string& objectText, const std::string& key, uint32_t defaultValue)
	{
		std::regex pattern("\"" + key + "\"\\s*:\\s*(\\d+)");
		std::smatch match;
		if (std::regex_search(objectText, match, pattern)) {
			return static_cast<uint32_t>(std::stoul(match[1].str()));
		}

		return defaultValue;
	}

	std::vector<GltfAccessor> ParseAccessors(const std::string& json)
	{
		std::vector<GltfAccessor> accessors;
		std::string block = ExtractArrayBlock(json, "accessors");
		std::vector<std::string> objects = SplitTopLevelObjects(block);

		for (const std::string& objectText : objects) {
			GltfAccessor accessor;
			accessor.bufferView = FindUIntValue(objectText, "bufferView");
			accessor.byteOffset = FindUIntValueOrDefault(objectText, "byteOffset", 0);
			accessor.componentType = FindUIntValue(objectText, "componentType");
			accessor.count = FindUIntValue(objectText, "count");
			accessor.type = FindStringValue(objectText, "type");
			accessors.push_back(accessor);
		}

		return accessors;
	}

	std::vector<GltfBufferView> ParseBufferViews(const std::string& json)
	{
		std::vector<GltfBufferView> bufferViews;
		std::string block = ExtractArrayBlock(json, "bufferViews");
		std::vector<std::string> objects = SplitTopLevelObjects(block);

		for (const std::string& objectText : objects) {
			GltfBufferView bufferView;
			bufferView.buffer = FindUIntValue(objectText, "buffer");
			bufferView.byteOffset = FindUIntValueOrDefault(objectText, "byteOffset", 0);
			bufferView.byteLength = FindUIntValue(objectText, "byteLength");
			bufferViews.push_back(bufferView);
		}

		return bufferViews;
	}

	void ParsePrimitiveAccessorIndices(
		const std::string& meshObject,
		uint32_t& positionAccessorIndex,
		uint32_t& normalAccessorIndex,
		uint32_t& texcoordAccessorIndex,
		uint32_t& indexAccessorIndex)
	{
		std::smatch match;

		bool found = std::regex_search(meshObject, match, std::regex("\"POSITION\"\\s*:\\s*(\\d+)"));
		assert(found);
		positionAccessorIndex = static_cast<uint32_t>(std::stoul(match[1].str()));

		found = std::regex_search(meshObject, match, std::regex("\"NORMAL\"\\s*:\\s*(\\d+)"));
		assert(found);
		normalAccessorIndex = static_cast<uint32_t>(std::stoul(match[1].str()));

		found = std::regex_search(meshObject, match, std::regex("\"TEXCOORD_0\"\\s*:\\s*(\\d+)"));
		assert(found);
		texcoordAccessorIndex = static_cast<uint32_t>(std::stoul(match[1].str()));

		found = std::regex_search(meshObject, match, std::regex("\"indices\"\\s*:\\s*(\\d+)"));
		assert(found);
		indexAccessorIndex = static_cast<uint32_t>(std::stoul(match[1].str()));
	}


	void ParsePrimitiveSkinAccessorIndices(
		const std::string& meshObject,
		uint32_t& jointsAccessorIndex,
		uint32_t& weightsAccessorIndex)
	{
		std::smatch match;

		bool found = std::regex_search(meshObject, match, std::regex("\"JOINTS_0\"\\s*:\\s*(\\d+)"));
		assert(found);
		jointsAccessorIndex = static_cast<uint32_t>(std::stoul(match[1].str()));

		found = std::regex_search(meshObject, match, std::regex("\"WEIGHTS_0\"\\s*:\\s*(\\d+)"));
		assert(found);
		weightsAccessorIndex = static_cast<uint32_t>(std::stoul(match[1].str()));
	}

	std::vector<float> ParseFloatArray(const std::string& objectText, const std::string& key)
	{
		std::vector<float> result;

		size_t keyPos = objectText.find("\"" + key + "\"");
		if (keyPos == std::string::npos) {
			return result;
		}

		size_t arrayBegin = objectText.find('[', keyPos);
		size_t arrayEnd = FindMatchingBracket(objectText, arrayBegin, '[', ']');
		std::string arrayText = objectText.substr(arrayBegin + 1, arrayEnd - arrayBegin - 1);

		std::stringstream ss(arrayText);
		std::string valueText;
		while (std::getline(ss, valueText, ',')) {
			if (!valueText.empty()) {
				result.push_back(std::stof(valueText));
			}
		}

		return result;
	}


	struct GltfNode {
		std::string name;                 // // node 名
		Vector3 translate = { 0.0f, 0.0f, 0.0f }; // // 平行移動
		Quaternion rotate = { 0.0f, 0.0f, 0.0f, 1.0f }; // // 回転
		Vector3 scale = { 1.0f, 1.0f, 1.0f }; // // 拡大率
		std::vector<uint32_t> children;   // // 子 node の index
	};

	std::vector<uint32_t> ParseUIntArray(const std::string& objectText, const std::string& key)
	{
		std::vector<uint32_t> result; // // 読み取った整数配列

		size_t keyPos = objectText.find("\"" + key + "\"");
		if (keyPos == std::string::npos) {
			return result; // // key が無ければ空配列
		}

		size_t arrayBegin = objectText.find('[', keyPos);
		size_t arrayEnd = FindMatchingBracket(objectText, arrayBegin, '[', ']');
		std::string arrayText = objectText.substr(arrayBegin + 1, arrayEnd - arrayBegin - 1);

		std::stringstream ss(arrayText);
		std::string valueText;
		while (std::getline(ss, valueText, ',')) {
			if (!valueText.empty()) {
				result.push_back(static_cast<uint32_t>(std::stoul(valueText)));
			}
		}

		return result;
	}

	Vector3 ParseVector3OrDefault(const std::string& objectText, const std::string& key, const Vector3& defaultValue)
	{
		size_t keyPos = objectText.find("\"" + key + "\"");
		if (keyPos == std::string::npos) {
			return defaultValue; // // key が無ければ既定値
		}

		size_t arrayBegin = objectText.find('[', keyPos);
		size_t arrayEnd = FindMatchingBracket(objectText, arrayBegin, '[', ']');
		std::string arrayText = objectText.substr(arrayBegin + 1, arrayEnd - arrayBegin - 1);

		std::stringstream ss(arrayText);
		std::string valueText;
		Vector3 result = defaultValue; // // 初期値を入れておく

		std::getline(ss, valueText, ',');
		result.x = std::stof(valueText);
		std::getline(ss, valueText, ',');
		result.y = std::stof(valueText);
		std::getline(ss, valueText, ',');
		result.z = std::stof(valueText);

		return result;
	}

	Quaternion ParseQuaternionOrDefault(const std::string& objectText, const std::string& key, const Quaternion& defaultValue)
	{
		size_t keyPos = objectText.find("\"" + key + "\"");
		if (keyPos == std::string::npos) {
			return defaultValue; // // key が無ければ既定値
		}

		size_t arrayBegin = objectText.find('[', keyPos);
		size_t arrayEnd = FindMatchingBracket(objectText, arrayBegin, '[', ']');
		std::string arrayText = objectText.substr(arrayBegin + 1, arrayEnd - arrayBegin - 1);

		std::stringstream ss(arrayText);
		std::string valueText;
		Quaternion result = defaultValue; // // 初期値を入れておく

		std::getline(ss, valueText, ',');
		result.x = std::stof(valueText);
		std::getline(ss, valueText, ',');
		result.y = std::stof(valueText);
		std::getline(ss, valueText, ',');
		result.z = std::stof(valueText);
		std::getline(ss, valueText, ',');
		result.w = std::stof(valueText);

		return result;
	}

	std::vector<GltfNode> ParseNodes(const std::string& jsonText)
	{
		std::vector<GltfNode> nodes; // // glTF から読み出した node 一覧

		std::string nodesBlock = ExtractArrayBlock(jsonText, "nodes");
		std::vector<std::string> nodeObjects = SplitTopLevelObjects(nodesBlock);

		for (const std::string& nodeObject : nodeObjects) {
			GltfNode node; // // 1 つ分の node 情報

			if (nodeObject.find("\"name\"") != std::string::npos) {
				node.name = FindStringValue(nodeObject, "name");
			}

			node.translate = ParseVector3OrDefault(
				nodeObject, "translation", Vector3{ 0.0f, 0.0f, 0.0f });
			node.rotate = ParseQuaternionOrDefault(
				nodeObject, "rotation", Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f });
			node.scale = ParseVector3OrDefault(
				nodeObject, "scale", Vector3{ 1.0f, 1.0f, 1.0f });
			node.children = ParseUIntArray(nodeObject, "children");

			nodes.push_back(node);
		}

		return nodes;
	}

	Node ConvertNode(const std::vector<GltfNode>& gltfNodes, uint32_t nodeIndex)
	{
		const GltfNode& gltfNode = gltfNodes[nodeIndex]; // // 元の glTF node を参照
		Node result; // // 変換後の engine node

		result.name = gltfNode.name; // // 名前をコピー
		result.transform.scale = gltfNode.scale; // // scale はそのまま使う

		// // glTF は右手系なので x 軸反転で左手系へ寄せる
		result.transform.translate = {
			-gltfNode.translate.x,
			 gltfNode.translate.y,
			 gltfNode.translate.z
		};

		// // 回転方向も合わせるため y,z を反転する
		result.transform.rotate = Normalize({
			 gltfNode.rotate.x,
			-gltfNode.rotate.y,
			-gltfNode.rotate.z,
			 gltfNode.rotate.w
			});

		// // 読み込んだ TRS から localMatrix を再構築する
		result.localMatrix = MakeAffineMatrix(
			result.transform.scale,
			result.transform.rotate,
			result.transform.translate
		);

		// // 子 node を再帰的に変換して children に詰める
		for (uint32_t childIndex : gltfNode.children) {
			result.children.push_back(ConvertNode(gltfNodes, childIndex));
		}

		return result;
	}

	uint32_t ParseRootNodeIndex(const std::string& jsonText)
	{
		std::string scenesBlock = ExtractArrayBlock(jsonText, "scenes");
		std::vector<std::string> sceneObjects = SplitTopLevelObjects(scenesBlock);
		assert(!sceneObjects.empty());

		std::vector<uint32_t> rootNodes = ParseUIntArray(sceneObjects[0], "nodes");
		assert(!rootNodes.empty());

		return rootNodes[0]; // // 最初の scene の最初の node を root とする
	}

	Matrix4x4 TransposeMatrix(const Matrix4x4& matrix)
	{
		Matrix4x4 result{};

		for (int row = 0; row < 4; ++row) {
			for (int column = 0; column < 4; ++column) {
				result.m[row][column] = matrix.m[column][row];
			}
		}

		return result;
	}

	Matrix4x4 ConvertGltfMatrixToEngineMatrix(const float* matrixValues)
	{
		Matrix4x4 gltfMatrix{};

		for (int row = 0; row < 4; ++row) {
			for (int column = 0; column < 4; ++column) {
				gltfMatrix.m[row][column] = matrixValues[row * 4 + column];
			}
		}

		// glTF の列優先行列をこのエンジンの行列表現へ合わせる
		return TransposeMatrix(gltfMatrix);
	}

	
}

void Model::Initialize(ModelCommon* modelCommon,
	const std::string& directoryPath,
	const std::string& filename)
{
	modelCommon_ = modelCommon;

	// 拡張子に応じてローダを切り替える
	modelData_ = LoadModelFile(directoryPath, filename);

	
	TextureManager::GetInstance()->LoadTexture(
		modelData_.material.textureFilePath);

	// 頂点バッファを初期化する
	InitializeVertexBuffer();

	// index バッファを初期化する
	InitializeIndexBuffer();

	// material を初期化する
	InitializeMaterial();

}

void Model::Draw()
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	for (size_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];

		// 頂点バッファを設定する
		commandList->IASetVertexBuffers(0, 1, &vertexBufferViews_[meshIndex]);

		if (!mesh.indices.empty()) {
			// index バッファを設定する
			commandList->IASetIndexBuffer(&indexBufferViews_[meshIndex]);

			// index を使って描画する
			commandList->DrawIndexedInstanced(
				static_cast<UINT>(mesh.indices.size()),
				1,
				0,
				0,
				0);
		} else {
			// index が無い場合は従来の描画にフォールバックする
			commandList->DrawInstanced(
				static_cast<UINT>(mesh.vertices.size()),
				1,
				0,
				0);
		}
	}
}

ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	std::string extension = std::filesystem::path(filename).extension().string();

	if (extension == ".obj") {
		return LoadObjFile(directoryPath, filename);
	}

	if (extension == ".gltf") {
		return LoadGltfFile(directoryPath, filename);
	}

	assert(false);
	return ModelData{};
}

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData;

	std::string mtlFileName;

	{
		std::ifstream file(directoryPath + "/" + filename);
		assert(file.is_open());

		std::string line;
		while (std::getline(file, line)) {
			if (line.rfind("mtllib", 0) == 0) {
				std::istringstream s(line);
				std::string id;
				s >> id >> mtlFileName;
				break;
			}
		}
	}

	if (!mtlFileName.empty()) {
		Object3d::LoadMaterialTemplateFile(
			directoryPath,
			mtlFileName,
			modelData.material);
	}

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

			// 面の頂点を今まで通り 3 頂点として追加する
			currentMesh.vertices.push_back(triangle[2]);
			currentMesh.vertices.push_back(triangle[1]);
			currentMesh.vertices.push_back(triangle[0]);
		} else if (identifier == "o" || identifier == "g") {
			if (!currentMesh.vertices.empty()) {
				// ひとまず頂点数ぶんの連番 index を作る
				currentMesh.indices.resize(currentMesh.vertices.size());

				for (uint32_t index = 0; index < currentMesh.indices.size(); ++index) {
					// 0, 1, 2, 3 ... の順で index を入れる
					currentMesh.indices[index] = index;
				}

				modelData.meshes.push_back(currentMesh);
				currentMesh = MeshData();
			}

			std::string meshName;
			s >> meshName;
			currentMesh.name = meshName;
		}
	}

	if (!currentMesh.vertices.empty()) {
		// 最後の mesh に対しても連番 index を作る
		currentMesh.indices.resize(currentMesh.vertices.size());

		for (uint32_t index = 0; index < currentMesh.indices.size(); ++index) {
			// 0, 1, 2, 3 ... の順で index を入れる
			currentMesh.indices[index] = index;
		}

		modelData.meshes.push_back(currentMesh);
	}

	return modelData;
}


ModelData Model::LoadGltfFile(const std::string& directoryPath, const std::string& filename)
{
	ModelData modelData;

	const std::string gltfFilePath = directoryPath + "/" + filename;
	const std::string jsonText = ReadTextFile(gltfFilePath);
	const std::vector<GltfNode> gltfNodes = ParseNodes(jsonText); // // glTF の node 配列を読む
	const std::filesystem::path gltfDirectory = std::filesystem::path(gltfFilePath).parent_path();
	const std::vector<GltfAccessor> accessors = ParseAccessors(jsonText);
	const std::vector<GltfBufferView> bufferViews = ParseBufferViews(jsonText);

	std::string buffersBlock = ExtractArrayBlock(jsonText, "buffers");
	std::vector<std::string> bufferObjects = SplitTopLevelObjects(buffersBlock);
	assert(!bufferObjects.empty());
	const std::string bufferUri = FindStringValue(bufferObjects[0], "uri");
	const std::vector<uint8_t> binary = ReadBinaryFile((gltfDirectory / bufferUri).string());

	std::string texturesBlock = ExtractArrayBlock(jsonText, "textures");
	std::vector<std::string> textureObjects = SplitTopLevelObjects(texturesBlock);
	assert(!textureObjects.empty());
	uint32_t imageIndex = FindUIntValue(textureObjects[0], "source");

	std::string imagesBlock = ExtractArrayBlock(jsonText, "images");
	std::vector<std::string> imageObjects = SplitTopLevelObjects(imagesBlock);
	assert(imageIndex < imageObjects.size());
	modelData.material.textureFilePath =
		(gltfDirectory / FindStringValue(imageObjects[imageIndex], "uri")).string();

	std::string meshesBlock = ExtractArrayBlock(jsonText, "meshes");
	std::vector<std::string> meshObjects = SplitTopLevelObjects(meshesBlock);
	assert(!meshObjects.empty());
	const std::string& meshObject = meshObjects[0];

	uint32_t positionAccessorIndex = 0;
	uint32_t normalAccessorIndex = 0;
	uint32_t texcoordAccessorIndex = 0;
	uint32_t indexAccessorIndex = 0;
	uint32_t jointsAccessorIndex = 0;
	uint32_t weightsAccessorIndex = 0;

	// POSITION / NORMAL / TEXCOORD_0 / indices の accessor を読む
	ParsePrimitiveAccessorIndices(
		meshObject,
		positionAccessorIndex,
		normalAccessorIndex,
		texcoordAccessorIndex,
		indexAccessorIndex);

	// JOINTS_0 / WEIGHTS_0 の accessor を読む
	ParsePrimitiveSkinAccessorIndices(
		meshObject,
		jointsAccessorIndex,
		weightsAccessorIndex);



	const GltfAccessor& positionAccessor = accessors[positionAccessorIndex];
	const GltfAccessor& normalAccessor = accessors[normalAccessorIndex];
	const GltfAccessor& texcoordAccessor = accessors[texcoordAccessorIndex];
	const GltfAccessor& indexAccessor = accessors[indexAccessorIndex];
	const GltfAccessor& jointsAccessor = accessors[jointsAccessorIndex];
	const GltfAccessor& weightsAccessor = accessors[weightsAccessorIndex];

	assert(positionAccessor.componentType == 5126);
	assert(normalAccessor.componentType == 5126);
	assert(texcoordAccessor.componentType == 5126);
	assert(indexAccessor.componentType == 5123);
	assert(jointsAccessor.componentType == 5121 || jointsAccessor.componentType == 5123);
	assert(weightsAccessor.componentType == 5126);

	const GltfBufferView& positionBufferView = bufferViews[positionAccessor.bufferView];
	const GltfBufferView& normalBufferView = bufferViews[normalAccessor.bufferView];
	const GltfBufferView& texcoordBufferView = bufferViews[texcoordAccessor.bufferView];
	const GltfBufferView& indexBufferView = bufferViews[indexAccessor.bufferView];
	const GltfBufferView& jointsBufferView = bufferViews[jointsAccessor.bufferView];
	const GltfBufferView& weightsBufferView = bufferViews[weightsAccessor.bufferView];

	const float* positions = reinterpret_cast<const float*>(
		binary.data() + positionBufferView.byteOffset + positionAccessor.byteOffset);
	const float* normals = reinterpret_cast<const float*>(
		binary.data() + normalBufferView.byteOffset + normalAccessor.byteOffset);
	const float* texcoords = reinterpret_cast<const float*>(
		binary.data() + texcoordBufferView.byteOffset + texcoordAccessor.byteOffset);
	const uint16_t* indices = reinterpret_cast<const uint16_t*>(
		binary.data() + indexBufferView.byteOffset + indexAccessor.byteOffset);
	const float* weights = reinterpret_cast<const float*>(
		binary.data() + weightsBufferView.byteOffset + weightsAccessor.byteOffset);

	const uint8_t* jointsU8 = nullptr;
	const uint16_t* jointsU16 = nullptr;

	if (jointsAccessor.componentType == 5121) {
		jointsU8 = reinterpret_cast<const uint8_t*>(
			binary.data() + jointsBufferView.byteOffset + jointsAccessor.byteOffset);
	} else {
		jointsU16 = reinterpret_cast<const uint16_t*>(
			binary.data() + jointsBufferView.byteOffset + jointsAccessor.byteOffset);
	}

	MeshData meshData;
	meshData.name = "GltfMesh";
	if (meshObject.find("\"name\"") != std::string::npos) {
		meshData.name = FindStringValue(meshObject, "name");
	}

	// glTF の頂点数ぶんだけ確保する
	meshData.vertices.resize(positionAccessor.count);

	for (uint32_t vertexIndex = 0; vertexIndex < positionAccessor.count; ++vertexIndex) {
		Vector4 position = {
			positions[vertexIndex * 3 + 0],
			positions[vertexIndex * 3 + 1],
			positions[vertexIndex * 3 + 2],
			1.0f
		};

		Vector2 texcoord = {
			texcoords[vertexIndex * 2 + 0],
			1.0f - texcoords[vertexIndex * 2 + 1]
		};

		Vector3 normal = {
			normals[vertexIndex * 3 + 0],
			normals[vertexIndex * 3 + 1],
			normals[vertexIndex * 3 + 2]
		};

		// 右手系から左手系へ変換する
		position.x *= -1.0f;
		normal.x *= -1.0f;


		meshData.vertices[vertexIndex].position = position;
		meshData.vertices[vertexIndex].texcoord = texcoord;
		meshData.vertices[vertexIndex].normal = normal;
		meshData.vertices[vertexIndex].pad = 0.0f;
	}

	// glTF の index をそのまま使う
	meshData.indices.resize(indexAccessor.count);

	for (uint32_t index = 0; index < indexAccessor.count; ++index) {
		// いったん glTF の index をそのままコピーする
		meshData.indices[index] = indices[index];
	}

	// skin 情報を読む
	std::string skinsBlock = ExtractArrayBlock(jsonText, "skins");
	std::vector<std::string> skinObjects = SplitTopLevelObjects(skinsBlock);
	assert(!skinObjects.empty());

	// 今は最初の skin を使う
	const std::string& skinObject = skinObjects[0];

	// skin に含まれる joint 一覧を読む
	std::vector<uint32_t> skinJointIndices = ParseUIntArray(skinObject, "joints");
	assert(!skinJointIndices.empty());

	// inverseBindMatrices accessor を読む
	uint32_t inverseBindMatricesAccessorIndex =
		FindUIntValue(skinObject, "inverseBindMatrices");
	const GltfAccessor& inverseBindAccessor =
		accessors[inverseBindMatricesAccessorIndex];
	const GltfBufferView& inverseBindBufferView =
		bufferViews[inverseBindAccessor.bufferView];

	// inverseBindMatrices は MAT4 / float の配列
	assert(inverseBindAccessor.componentType == 5126);
	assert(inverseBindAccessor.type == "MAT4");

	const float* inverseBindMatrices = reinterpret_cast<const float*>(
		binary.data() + inverseBindBufferView.byteOffset + inverseBindAccessor.byteOffset);

	// glTF の joint index から joint 名へ引けるようにする
	std::vector<std::string> jointNames;
	jointNames.resize(skinJointIndices.size());

	for (uint32_t jointIndex = 0; jointIndex < skinJointIndices.size(); ++jointIndex) {
		uint32_t nodeIndex = skinJointIndices[jointIndex];
		assert(nodeIndex < gltfNodes.size());

		jointNames[jointIndex] = gltfNodes[nodeIndex].name;
	}

	// 各 joint の inverseBindPoseMatrix を先に作る
	for (uint32_t jointIndex = 0; jointIndex < skinJointIndices.size(); ++jointIndex) {
		JointWeightData& jointWeightData =
			modelData.skinClusterData[jointNames[jointIndex]];

		const float* matrixValues = inverseBindMatrices + jointIndex * 16;

		Matrix4x4 inverseBindPoseMatrix = ConvertGltfMatrixToEngineMatrix(matrixValues);

		// glTF の 4x4 行列を読む
		// glTF の 4x4 行列をいったんそのまま詰める

		// 今はまず読み込み結果をそのまま使う
		jointWeightData.inverseBindPoseMatrix = inverseBindPoseMatrix;


	}

	// 各頂点の JOINTS_0 / WEIGHTS_0 を joint 単位へばらす
	for (uint32_t vertexIndex = 0; vertexIndex < positionAccessor.count; ++vertexIndex) {
		for (uint32_t influenceIndex = 0; influenceIndex < 4; ++influenceIndex) {
			float weight = weights[vertexIndex * 4 + influenceIndex];

			// 重み 0 は無視する
			if (weight == 0.0f) {
				continue;
			}

			uint32_t jointIndex = 0;
			if (jointsU8) {
				jointIndex = jointsU8[vertexIndex * 4 + influenceIndex];
			} else {
				jointIndex = jointsU16[vertexIndex * 4 + influenceIndex];
			}

			assert(jointIndex < jointNames.size());

			VertexWeightData vertexWeightData{};
			vertexWeightData.weight = weight;
			vertexWeightData.vertexIndex = vertexIndex;

			modelData.skinClusterData[jointNames[jointIndex]]
				.vertexWeights.push_back(vertexWeightData);
		}
	}

	modelData.meshes.push_back(meshData);

	uint32_t rootNodeIndex = ParseRootNodeIndex(jsonText); // // root node の index を取得
	modelData.rootNode = ConvertNode(gltfNodes, rootNodeIndex); // // root から node 階層を構築

	return modelData;
}

void Model::InitializeVertexBuffer()
{
	vertexSrvIndices_.resize(modelData_.meshes.size());
	vertexSrvHandlesGPU_.resize(modelData_.meshes.size());

	DirectXCommon* dxCommon = modelCommon_->GetDxCommon();

	vertexBuffers_.clear();
	vertexBufferViews_.clear();

	vertexBuffers_.resize(modelData_.meshes.size());
	vertexBufferViews_.resize(modelData_.meshes.size());

	for (size_t i = 0; i < modelData_.meshes.size(); ++i) {
		const auto& mesh = modelData_.meshes[i];
		const auto& vertices = mesh.vertices;

		size_t bufferSize = sizeof(VertexData) * vertices.size();

		vertexBuffers_[i] = dxCommon->CreateBufferResource(bufferSize);

		VertexData* mapped = nullptr;
		vertexBuffers_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		std::memcpy(mapped, vertices.data(), bufferSize);
		vertexBuffers_[i]->Unmap(0, nullptr);

		vertexBufferViews_[i].BufferLocation = vertexBuffers_[i]->GetGPUVirtualAddress();
		vertexBufferViews_[i].SizeInBytes = static_cast<UINT>(bufferSize);
		vertexBufferViews_[i].StrideInBytes = sizeof(VertexData);

		// 元頂点バッファ用の SRV index を確保する
		vertexSrvIndices_[i] = modelCommon_->GetSrvManager()->Allocate();

		// 元頂点バッファ用の GPU ハンドルを保存する
		vertexSrvHandlesGPU_[i] =
			modelCommon_->GetSrvManager()->GetGPUDescriptorHandle(vertexSrvIndices_[i]);

		// 元頂点バッファを StructuredBuffer の SRV として作成する
		modelCommon_->GetSrvManager()->CreateSRVforStructuredBuffer(
			vertexSrvIndices_[i],
			vertexBuffers_[i].Get(),
			static_cast<UINT>(vertices.size()),
			sizeof(VertexData));


	}

	// ComputeShader 用に全 mesh の頂点を 1 本へ連結する
	std::vector<VertexData> combinedVertices;

	for (const auto& mesh : modelData_.meshes) {
		combinedVertices.insert(
			combinedVertices.end(),
			mesh.vertices.begin(),
			mesh.vertices.end());
	}

	size_t combinedBufferSize = sizeof(VertexData) * combinedVertices.size();

	combinedVertexBuffer_ = dxCommon->CreateBufferResource(combinedBufferSize);

	VertexData* mappedCombined = nullptr;
	combinedVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCombined));
	std::memcpy(mappedCombined, combinedVertices.data(), combinedBufferSize);
	combinedVertexBuffer_->Unmap(0, nullptr);

	// 全頂点連結バッファ用の SRV index を確保する
	combinedVertexSrvIndex_ = modelCommon_->GetSrvManager()->Allocate();

	// 全頂点連結バッファ用の GPU ハンドルを保存する
	combinedVertexSrvHandleGPU_ =
		modelCommon_->GetSrvManager()->GetGPUDescriptorHandle(combinedVertexSrvIndex_);

	// 全頂点連結バッファを StructuredBuffer の SRV として作成する
	modelCommon_->GetSrvManager()->CreateSRVforStructuredBuffer(
		combinedVertexSrvIndex_,
		combinedVertexBuffer_.Get(),
		static_cast<UINT>(combinedVertices.size()),
		sizeof(VertexData));

}

void Model::InitializeMaterial()
{
	DirectXCommon* dxCommon = modelCommon_->GetDxCommon();

	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->lightingType = static_cast<int>(LightingType::HalfLambert);
	materialData_->environmentCoefficient = 0.0f;
	materialData_->uvTransform = MakeIdentity4x4();
}

void Model::DrawInstanced(UINT instanceCount)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	for (size_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];

		// 頂点バッファを設定する
		commandList->IASetVertexBuffers(0, 1, &vertexBufferViews_[meshIndex]);

		if (!mesh.indices.empty()) {
			// index バッファを設定する
			commandList->IASetIndexBuffer(&indexBufferViews_[meshIndex]);

			// index を使ってインスタンシング描画する
			commandList->DrawIndexedInstanced(
				static_cast<UINT>(mesh.indices.size()),
				instanceCount,
				0,
				0,
				0);
		} else {
			// index が無い場合は従来の描画にフォールバックする
			commandList->DrawInstanced(
				static_cast<UINT>(mesh.vertices.size()),
				instanceCount,
				0,
				0);
		}
	}
}


void Model::InitializeIndexBuffer()
{
	DirectXCommon* dxCommon = modelCommon_->GetDxCommon();

	// mesh 数に合わせて確保する
	indexBuffers_.resize(modelData_.meshes.size());
	indexBufferViews_.resize(modelData_.meshes.size());

	for (size_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];

		// index が無ければ何もしない
		if (mesh.indices.empty()) {
			continue;
		}

		size_t bufferSize = sizeof(uint32_t) * mesh.indices.size();

		// index buffer を作る
		indexBuffers_[meshIndex] = dxCommon->CreateBufferResource(bufferSize);

		uint32_t* mappedIndex = nullptr;

		// 書き込み先を取得する
		indexBuffers_[meshIndex]->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&mappedIndex));

		// index 配列の内容をコピーする
		std::memcpy(mappedIndex, mesh.indices.data(), bufferSize);

		// Map を閉じる
		indexBuffers_[meshIndex]->Unmap(0, nullptr);

		// index buffer view を設定する
		indexBufferViews_[meshIndex].BufferLocation =
			indexBuffers_[meshIndex]->GetGPUVirtualAddress();

		indexBufferViews_[meshIndex].SizeInBytes =
			static_cast<UINT>(bufferSize);

		indexBufferViews_[meshIndex].Format = DXGI_FORMAT_R32_UINT;
	}
}

void Model::Draw(const D3D12_VERTEX_BUFFER_VIEW& influenceBufferView)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	for (size_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];

		// 頂点データと influence の 2 本を IA に渡す
		D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
			vertexBufferViews_[meshIndex],
			influenceBufferView
		};

		commandList->IASetVertexBuffers(0, 2, vbvs);

		if (!mesh.indices.empty()) {
			// index バッファを設定する
			commandList->IASetIndexBuffer(&indexBufferViews_[meshIndex]);

			// index を使って描画する
			commandList->DrawIndexedInstanced(
				static_cast<UINT>(mesh.indices.size()),
				1,
				0,
				0,
				0);
		} else {
			// index が無い場合は通常描画にフォールバックする
			commandList->DrawInstanced(
				static_cast<UINT>(mesh.vertices.size()),
				1,
				0,
				0);
		}
	}
}

void Model::DrawInstanced(UINT instanceCount, const D3D12_VERTEX_BUFFER_VIEW& influenceBufferView)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	for (size_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];

		// 頂点データと influence の 2 本を IA に渡す
		D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
			vertexBufferViews_[meshIndex],
			influenceBufferView
		};

		commandList->IASetVertexBuffers(0, 2, vbvs);

		if (!mesh.indices.empty()) {
			// index バッファを設定する
			commandList->IASetIndexBuffer(&indexBufferViews_[meshIndex]);

			// index を使ってインスタンシング描画する
			commandList->DrawIndexedInstanced(
				static_cast<UINT>(mesh.indices.size()),
				instanceCount,
				0,
				0,
				0);
		} else {
			// index が無い場合は通常描画にフォールバックする
			commandList->DrawInstanced(
				static_cast<UINT>(mesh.vertices.size()),
				instanceCount,
				0,
				0);
		}
	}
}

// ComputeShader で作った変形済み頂点バッファを使って描画する
void Model::DrawWithSkinnedVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& skinnedVertexBufferView)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	for (size_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];

		// ComputeShader が書いた変形済み頂点バッファを使う
		commandList->IASetVertexBuffers(0, 1, &skinnedVertexBufferView);

		if (!mesh.indices.empty()) {
			commandList->IASetIndexBuffer(&indexBufferViews_[meshIndex]);

			commandList->DrawIndexedInstanced(
				static_cast<UINT>(mesh.indices.size()),
				1,
				0,
				0,
				0);
		} else {
			commandList->DrawInstanced(
				static_cast<UINT>(mesh.vertices.size()),
				1,
				0,
				0);
		}
	}
}

// ComputeShader で作った変形済み頂点バッファを使って instancing 描画する
void Model::DrawInstancedWithSkinnedVertexBuffer(
	UINT instanceCount,
	const D3D12_VERTEX_BUFFER_VIEW& skinnedVertexBufferView)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	for (size_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex) {
		const MeshData& mesh = modelData_.meshes[meshIndex];

		// ComputeShader が書いた変形済み頂点バッファを使う
		commandList->IASetVertexBuffers(0, 1, &skinnedVertexBufferView);

		if (!mesh.indices.empty()) {
			commandList->IASetIndexBuffer(&indexBufferViews_[meshIndex]);

			commandList->DrawIndexedInstanced(
				static_cast<UINT>(mesh.indices.size()),
				instanceCount,
				0,
				0,
				0);
		} else {
			commandList->DrawInstanced(
				static_cast<UINT>(mesh.vertices.size()),
				instanceCount,
				0,
				0);
		}
	}
}
