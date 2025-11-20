#include "Object3d.h"
#include "Object3dCommon.h"

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    object3dCommon_ = object3dCommon;

    // OBJ 読み込み
    modelData_ = LoadObjFile("Resources", "plane.obj");

    // ==========================
    // テクスチャ読み込み（スライド部分）
    // ==========================
    // とりあえず手動でテクスチャパスを設定しておく
    // → 自分が使いたい png に合わせて変えてOK
    modelData_.material.textureFilePath = "Resources/uvChecker.png";

    // .obj が参照しているテクスチャファイルを読み込み
    TextureManager::GetInstance()->LoadTexture(
        modelData_.material.textureFilePath);

    // 読み込んだテクスチャの番号を取得
    modelData_.material.textureIndex =
        TextureManager::GetInstance()->GetTextureIndexByFilePath(
            modelData_.material.textureFilePath);

    transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    cameraTransform = { {1.0f,1.0f,1.0f},{0.3f,0.0f,0.0f},{0.0f,4.0f,-10.0f} };

    // 残りの初期化
    InitializeVertexBuffer();
    InitializeMaterial();
    InitializeTransformationMatrix();
    InitializeDirectionalLight();
}

void Object3d::Update()
{
    assert(transformationMatrixData_);   // 初期化済み前提

    // ① Transform → WorldMatrix
    Matrix4x4 worldMatrix =
        MakeAffineMatrix(transform.scale,
            transform.rotate,
            transform.translate);

    // ② cameraTransform → cameraMatrix
    Matrix4x4 cameraMatrix =
        MakeAffineMatrix(cameraTransform.scale,
            cameraTransform.rotate,
            cameraTransform.translate);

    // ③ cameraMatrix → viewMatrix（逆行列）
    Matrix4x4 viewMatrix = Inverse(cameraMatrix);

    // ④ projectionMatrix（射影行列）
    Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
        0.45f,
        float(WinApp::kClientWidth) / float(WinApp::kClientHeight),
        0.1f, 100.0f);

    // ⑤ WVP と World を定数バッファに書き込む
    transformationMatrixData_->WVP =
        Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
    transformationMatrixData_->World = worldMatrix;
}

// Object3d.cpp

void Object3d::Draw()
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // ---------- VertexBufferView を設定 ----------
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

    // ---------- マテリアル用 CBuffer の場所を設定 ----------
    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());

    // ---------- 平行光源 CBuffer の場所を設定 ----------
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());

    // ---------- 座標変換行列 CBuffer の場所を設定 ----------
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());

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


MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath,
    const std::string& filename) {
    MaterialData material{};
    material.textureFilePath = directoryPath + "/" + filename; // 必要なら
    return material;
}

ModelData Object3d::LoadObjFile(const std::string& directoryPath, const std::string& filename)
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

void Object3d::InitializeVertexBuffer()
{
   // Object3dCommon 経由で DirectXCommon を取ってくる想定
   DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // Ensure the method name matches the declaration in Object3dCommon

   // ここではとりあえず最初のメッシュだけを使う
   const std::vector<VertexData>& vertices = modelData_.meshes[0].vertices;

   // リソース作成
   size_t bufferSize = sizeof(VertexData) * vertices.size();
   vertexBuffer = dxCommon->CreateBufferResource(bufferSize);

   // データを書き込む
   vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
   std::memcpy(vertexData, vertices.data(), bufferSize);
   vertexBuffer->Unmap(0, nullptr);

   // ビューの設定
   vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
   vertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
   vertexBufferView.StrideInBytes = sizeof(VertexData);
}

void Object3d::InitializeMaterial()
{
    // Object3dCommon から DirectXCommon を取得
    // ゲッター名は自分のクラスに合わせて直してね
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

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

void Object3d::InitializeTransformationMatrix()
{
    // Object3dCommon から DirectXCommon を取得
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon(); // 実プロジェクトの関数名に合わせて

    // バッファリソース作成
    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    // マップしてポインタ取得
    transformationMatrixResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));

    // 初期値を書き込む
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}

void Object3d::InitializeDirectionalLight()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    // バッファ作成
    directionalLightResource_ =
        dxCommon->CreateBufferResource(sizeof(DirectionalLight));

    // マップしてポインタ取得
    directionalLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&directionalLightData_));

    // 初期値設定（main.cpp と同じ）
    directionalLightData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f);
    directionalLightData_->intensity = 4.0f;
}
