#include "Model.h"

void Model::Initialize(ModelCommon* modelCommon)
{
	// 引き渡し
	modelCommon_ = modelCommon;

	// OBJ 読み込み
	modelData_ = LoadObjFile("Resources", "plane.obj");
    modelData_.material.textureFilePath = "Resources/uvChecker.png";

    // 残りの初期化
    InitializeVertexBuffer();
    InitializeMaterial();

    // .obj が参照しているテクスチャファイルを読み込み
    TextureManager::GetInstance()->LoadTexture(
        modelData_.material.textureFilePath);

    // 読み込んだテクスチャの番号を取得
    modelData_.material.textureIndex =
        TextureManager::GetInstance()->GetTextureIndexByFilePath(
            modelData_.material.textureFilePath);

}

void Model::Draw()
{
    assert(modelCommon_);
    DirectXCommon* dxCommon = modelCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // ---------- VertexBufferView を設定 ----------
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // ---------- マテリアル用 CBuffer の場所を設定 ----------
    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());

    // ---------- SRV の DescriptorTable の先頭を設定 ----------
    TextureManager* texMan = TextureManager::GetInstance();
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle =
        texMan->GetSrvHandleGPU(modelData_.material.textureIndex);

    commandList->SetGraphicsRootDescriptorTable(3, textureHandle);

    // ---------- 描画！（DrawCall） ----------
    if (!modelData_.meshes.empty()) {
        const MeshData& mesh = modelData_.meshes[0];   // ひとまず 0 番メッシュだけ
        commandList->DrawInstanced(
            static_cast<UINT>(mesh.vertices.size()), // 頂点数
            1,                                        // インスタンス数
            0, 0);
    }
}

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
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

            // ★ ここは modelData_ ではなく modelData
            currentMesh.vertices.push_back(triangle[2]);
            currentMesh.vertices.push_back(triangle[1]);
            currentMesh.vertices.push_back(triangle[0]);
        } else if (identifier == "o" || identifier == "g") {
            if (!currentMesh.vertices.empty()) {
                modelData.meshes.push_back(currentMesh);   // ★ 修正
                currentMesh = MeshData();
            }

            std::string meshName;
            s >> meshName;
            currentMesh.name = meshName;
        }
    }

    if (!currentMesh.vertices.empty()) {
        modelData.meshes.push_back(currentMesh);           // ★ 修正
    }

    return modelData;                                      // ★ ローカルを返す
}

void Model::InitializeVertexBuffer()
{
    // Object3dCommon 経由で DirectXCommon を取ってくる想定
    DirectXCommon* dxCommon = modelCommon_->GetDxCommon(); // Ensure the method name matches the declaration in Object3dCommon

    // ここではとりあえず最初のメッシュだけを使う
    const std::vector<VertexData>& vertices = modelData_.meshes[0].vertices;

    // リソース作成
    size_t bufferSize = sizeof(VertexData) * vertices.size();
    vertexBuffer_ = dxCommon->CreateBufferResource(bufferSize);

    // データを書き込む
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    std::memcpy(vertexData_, vertices.data(), bufferSize);
    vertexBuffer_->Unmap(0, nullptr);

    // ビューの設定
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Model::InitializeMaterial()
{
    // Object3dCommon から DirectXCommon を取得
    // ゲッター名は自分のクラスに合わせて直してね
    DirectXCommon* dxCommon = modelCommon_->GetDxCommon();

    // ▼ main.cpp から持ってきた処理 ▼

    // マテリアル用の定数バッファリソースを作成
    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));

    // マップしてアドレス取得
    materialResource_->Map(0, nullptr,
        reinterpret_cast<void**>(&materialData_));

    // マテリアルデータの初期値を書き込み
    materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    // main.cpp でやっていたのと同じように lightingType を設定
    // （Lambert を初期値にしておく例）
    materialData_->lightingType =
        static_cast<int>(LightingType::Lambert);

    // UV 行列は単位行列
    materialData_->uvTransform = MakeIdentity4x4();
}
