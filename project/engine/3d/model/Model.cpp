#include "Model.h"

#include <cstring>
#include <filesystem>
#include <regex>

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

std::string ExtractArrayBlock(const std::string& json, const std::string& key)
{
    size_t keyPos = json.find("\"" + key + "\"");
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

}

void Model::Initialize(ModelCommon* modelCommon,
    const std::string& directoryPath,
    const std::string& filename)
{
    modelCommon_ = modelCommon;

    // 拡張子に応じてローダを切り替える
    modelData_ = LoadModelFile(directoryPath, filename);

    // 笆ｼ 繝・け繧ｹ繝√Ε隱ｭ縺ｿ霎ｼ縺ｿ
    TextureManager::GetInstance()->LoadTexture(
        modelData_.material.textureFilePath);

    // 笆ｼ 鬆らせ繝舌ャ繝輔ぃ蛻晄悄蛹・
    InitializeVertexBuffer();

    // 笆ｼ 繝槭ユ繝ｪ繧｢繝ｫ蛻晄悄蛹・
    InitializeMaterial();
}

void Model::Draw()
{
    auto* dxCommon = modelCommon_->GetDxCommon();
    auto* commandList = dxCommon->GetCommandList();

    auto* srvManager = TextureManager::GetInstance()->GetSrvManager();

    ID3D12DescriptorHeap* heaps[] = {
        srvManager->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle =
        TextureManager::GetInstance()->GetSrvHandleGPU(
            modelData_.material.textureFilePath);

    commandList->SetGraphicsRootDescriptorTable(4, textureHandle);

    for (size_t i = 0; i < modelData_.meshes.size(); ++i) {
        commandList->IASetVertexBuffers(0, 1, &vertexBufferViews_[i]);
        commandList->DrawInstanced(
            static_cast<UINT>(modelData_.meshes[i].vertices.size()), 1, 0, 0);
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

    if (!currentMesh.vertices.empty()) {
        modelData.meshes.push_back(currentMesh);
    }

    return modelData;
}

ModelData Model::LoadGltfFile(const std::string& directoryPath, const std::string& filename)
{
    ModelData modelData;

    const std::string gltfFilePath = directoryPath + "/" + filename;
    const std::string jsonText = ReadTextFile(gltfFilePath);
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
    ParsePrimitiveAccessorIndices(
        meshObject,
        positionAccessorIndex,
        normalAccessorIndex,
        texcoordAccessorIndex,
        indexAccessorIndex);

    const GltfAccessor& positionAccessor = accessors[positionAccessorIndex];
    const GltfAccessor& normalAccessor = accessors[normalAccessorIndex];
    const GltfAccessor& texcoordAccessor = accessors[texcoordAccessorIndex];
    const GltfAccessor& indexAccessor = accessors[indexAccessorIndex];

    assert(positionAccessor.componentType == 5126);
    assert(normalAccessor.componentType == 5126);
    assert(texcoordAccessor.componentType == 5126);
    assert(indexAccessor.componentType == 5123);

    const GltfBufferView& positionBufferView = bufferViews[positionAccessor.bufferView];
    const GltfBufferView& normalBufferView = bufferViews[normalAccessor.bufferView];
    const GltfBufferView& texcoordBufferView = bufferViews[texcoordAccessor.bufferView];
    const GltfBufferView& indexBufferView = bufferViews[indexAccessor.bufferView];

    const float* positions = reinterpret_cast<const float*>(
        binary.data() + positionBufferView.byteOffset + positionAccessor.byteOffset);
    const float* normals = reinterpret_cast<const float*>(
        binary.data() + normalBufferView.byteOffset + normalAccessor.byteOffset);
    const float* texcoords = reinterpret_cast<const float*>(
        binary.data() + texcoordBufferView.byteOffset + texcoordAccessor.byteOffset);
    const uint16_t* indices = reinterpret_cast<const uint16_t*>(
        binary.data() + indexBufferView.byteOffset + indexAccessor.byteOffset);

    MeshData meshData;
    meshData.name = "GltfMesh";
    if (meshObject.find("\"name\"") != std::string::npos) {
        meshData.name = FindStringValue(meshObject, "name");
    }

    for (uint32_t i = 0; i < indexAccessor.count; i += 3) {
        VertexData triangle[3]{};

        for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
            uint32_t vertexIndex = indices[i + faceVertex];

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

            // 既存のobj読込と同じ向きに揃える
            position.x *= -1.0f;
            normal.x *= -1.0f;

            float rad = 3.141592f;
            float x = position.x;
            float z = position.z;
            position.x = x * cos(rad) - z * sin(rad);
            position.z = x * sin(rad) + z * cos(rad);

            triangle[faceVertex].position = position;
            triangle[faceVertex].texcoord = texcoord;
            triangle[faceVertex].normal = normal;
            triangle[faceVertex].pad = 0.0f;
        }

        meshData.vertices.push_back(triangle[2]);
        meshData.vertices.push_back(triangle[1]);
        meshData.vertices.push_back(triangle[0]);
    }

    modelData.meshes.push_back(meshData);
    return modelData;
}

void Model::InitializeVertexBuffer()
{
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
    }
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
    assert(modelCommon_);
    ID3D12GraphicsCommandList* commandList =
        modelCommon_->GetDxCommon()->GetCommandList();

    for (size_t i = 0; i < modelData_.meshes.size(); ++i) {
        commandList->IASetVertexBuffers(0, 1, &vertexBufferViews_[i]);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        commandList->DrawInstanced(
            static_cast<UINT>(modelData_.meshes[i].vertices.size()),
            instanceCount,
            0, 0);
    }
}
