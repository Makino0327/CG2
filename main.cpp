// 必要なヘッダー
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <cstdint>
#include <cassert>
#include <initguid.h>
#include <dxcapi.h>
#include <string>
#include <format>
#include <cmath>
#include <DirectXMath.h>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h" // DirectXTexヘッダーをインクルード
#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>


// 必要なライブラリリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


// Vector4型を定義する
struct Vector4 {
	float x, y, z, w;
	Vector4() = default;
	Vector4(float X, float Y, float Z, float W) :x(X), y(Y), z(Z), w(W) {}
};


struct Vector3 {
	float x, y, z;
};

struct Vector2 {
	float x, y;
};

struct Matrix4x4 {
	float m[4][4];
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct Node {
	Matrix4x4 localMatrix;   // Nodeのローカル変換
	std::string name;        // Node名
	std::vector<Node> children; // 子Node
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	float pad;
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	float shininess;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose; // ★追加
};


struct DirectionalLight {
	Vector4 color;        // ライトの色
	Vector3 direction;    // ライトの向き（単位ベクトル）
	float intensity;      // 強度
};

struct CameraForGPU
{
	Vector3 worldPosition;
	float padding; // 16byte揃え（float3だけだとズレるので）
};

struct ObjModelData {
	std::vector<VertexData> vertices; // 三角形リスト用（非indexed）
	std::vector<uint32_t>   indices;    // indexed用
	std::string diffuseTexturePath;  
	Node rootNode;     // 取得できたら入る
};

Matrix4x4 Transpose(const Matrix4x4& m) {
	Matrix4x4 r{};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			r.m[row][col] = m.m[col][row];
		}
	}
	return r;
}

/// <summary>
///ディスクリプタヒープを作成
/// </summary>
/// <param name="device">ディスクリプタヒープを作成する対象の ID3D12Device</param>
/// <param name="heapType">作成するディスクリプタヒープの種類（CBV/SRV/UAV など）</param>
/// <param name="numDescriptors">ディスクリプタの数</param>
/// <param name="shaderVisible">シェーダーから参照可能にするかどうか</param>
/// <returns>作成された ID3D12DescriptorHeap のポインタ。失敗した場合は nullptr。</returns>
ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
/// <summary>
/// 単位行列
/// </summary>
/// <returns>単位行列</returns>
Matrix4x4 MakeIdentity4x4();
/// <summary>
/// スケール、回転、平行移動の各要素からアフィン変換行列（4x4）を生成。
/// </summary>
/// <param name="scale">拡大縮小を表すスケールベクトル。</param>
/// <param name="rotate">回転を表すオイラー角（ラジアン）ベクトル。</param>
/// <param name="translate">位置を表す平行移動ベクトル。</param>
/// <returns>アフィン変換を表す 4x4 行列。</returns>
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
/// <summary>
/// 垂直方向の視野角、アスペクト比、近距離および遠距離クリップ面を元に透視投影行列（4x4）を生成。
/// </summary>
/// <param name="fovY">垂直方向の視野角（ラジアン単位）。</param>
/// <param name="aspect">アスペクト比（横幅 ÷ 高さ）。</param>
/// <param name="nearZ">近距離クリップ面の距離。</param>
/// <param name="farZ">遠距離クリップ面の距離。</param>
/// <returns>透視投影を表す 4x4 行列。</returns>
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearZ, float farZ);
/// <summary>
/// 2つの 4x4 行列の積を計算し、合成された変換行列を返します。
/// </summary>
/// <param name="a">左側の行列（先に適用される変換）。</param>
/// <param name="b">右側の行列（後に適用される変換）。</param>
/// <returns>掛け算の結果となる 4x4 行列。</returns>
Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b);
/// <summary>
/// 指定された 4x4 行列の逆行列を計算して返します。
/// </summary>
/// <param name="m">逆行列を求める対象の 4x4 行列。</param>
/// <returns>指定された行列の逆行列（Matrix4x4 型）。</returns>
Matrix4x4 Inverse(const Matrix4x4& m);

Vector3 Normalize(const Vector3& v) {
	float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	if (length == 0.0f) return { 0.0f, 0.0f, 0.0f };
	return { v.x / length, v.y / length, v.z / length };
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearZ, float farZ);
DirectX::ScratchImage LoadTexture(const std::string& filePath);
ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);
void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);
ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}
ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height)
{
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1; // MipMapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行きor配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM状に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 深度値のクリア値
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度値のフォーマット

	// Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue, // Clear最適値
		IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));

	return resource;
}
IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler);

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE  handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}


static int ResolveObjIndex(int idx, int count) {
	// OBJ: 1-based。負数は末尾基準
	if (idx > 0) return idx - 1;
	if (idx < 0) return count + idx;
	return -1; // 0は無効
}

Node ReadNode(aiNode* node) {
	Node result;

	aiMatrix4x4 aiLocalMatrix = node->mTransformation;

	// ★Assimpの並び → 自前行列の並びに合わせる
	aiLocalMatrix.Transpose();

	// localMatrix をコピー
	result.localMatrix.m[0][0] = aiLocalMatrix.a1;
	result.localMatrix.m[0][1] = aiLocalMatrix.a2;
	result.localMatrix.m[0][2] = aiLocalMatrix.a3;
	result.localMatrix.m[0][3] = aiLocalMatrix.a4;

	result.localMatrix.m[1][0] = aiLocalMatrix.b1;
	result.localMatrix.m[1][1] = aiLocalMatrix.b2;
	result.localMatrix.m[1][2] = aiLocalMatrix.b3;
	result.localMatrix.m[1][3] = aiLocalMatrix.b4;

	result.localMatrix.m[2][0] = aiLocalMatrix.c1;
	result.localMatrix.m[2][1] = aiLocalMatrix.c2;
	result.localMatrix.m[2][2] = aiLocalMatrix.c3;
	result.localMatrix.m[2][3] = aiLocalMatrix.c4;

	result.localMatrix.m[3][0] = aiLocalMatrix.d1;
	result.localMatrix.m[3][1] = aiLocalMatrix.d2;
	result.localMatrix.m[3][2] = aiLocalMatrix.d3;
	result.localMatrix.m[3][3] = aiLocalMatrix.d4;

	// 名前
	result.name = node->mName.C_Str();

	// 子
	result.children.resize(node->mNumChildren);
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}


ObjModelData LoadObjFile(const std::string& filePath) {
	ObjModelData model{};

	std::ifstream file(filePath);
	assert(file.is_open());

	std::vector<Vector3> positions; // v
	std::vector<Vector2> texcoords; // vt
	std::vector<Vector3> normals;   // vn

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty()) continue;
		if (line[0] == '#') continue;

		std::istringstream iss(line);
		std::string id;
		iss >> id;

		if (id == "v") {
			Vector3 p{};
			iss >> p.x >> p.y >> p.z;

			// ★右手OBJ → 左手へ（Z反転）
			p.z *= -1.0f;

			positions.push_back(p);
		} else if (id == "vt") {
			Vector2 uv{};
			iss >> uv.x >> uv.y;

			// ★DirectX系に合わせてV反転
			uv.y = 1.0f - uv.y;

			texcoords.push_back(uv);
		} else if (id == "vn") {
			Vector3 n{};
			iss >> n.x >> n.y >> n.z;

			// ★右手OBJ → 左手へ（Z反転）
			n.z *= -1.0f;

			normals.push_back(Normalize(n));
		} else if (id == "f") {
			// f は "v/vt/vn" or "v//vn" or "v/vt" or "v" が来る
			// 面は3以上あり得るので、fan triangulationする
			struct Idx { int v = -1, vt = -1, vn = -1; };
			std::vector<Idx> face;

			std::string token;
			while (iss >> token) {
				Idx idx{};

				// token を / 区切りで分解
				// 例: "12/3/9", "12//9", "12/3", "12"
				int slash1 = (int)token.find('/');
				if (slash1 == (int)std::string::npos) {
					// v
					idx.v = std::stoi(token);
				} else {
					std::string a = token.substr(0, slash1);
					idx.v = a.empty() ? 0 : std::stoi(a);

					int slash2 = (int)token.find('/', slash1 + 1);
					if (slash2 == (int)std::string::npos) {
						// v/vt
						std::string b = token.substr(slash1 + 1);
						idx.vt = b.empty() ? 0 : std::stoi(b);
					} else {
						// v/vt/vn or v//vn
						std::string b = token.substr(slash1 + 1, slash2 - (slash1 + 1));
						std::string c = token.substr(slash2 + 1);
						idx.vt = b.empty() ? 0 : std::stoi(b);
						idx.vn = c.empty() ? 0 : std::stoi(c);
					}
				}

				face.push_back(idx);
			}

			if (face.size() < 3) continue;

			auto makeVertex = [&](const Idx& fidx) -> VertexData {
				VertexData out{};

				int pi = ResolveObjIndex(fidx.v, (int)positions.size());
				assert(pi >= 0 && pi < (int)positions.size());
				Vector3 p = positions[pi];

				Vector2 uv{ 0.0f, 0.0f };
				if (!texcoords.empty() && fidx.vt != -1 && fidx.vt != 0) {
					int ti = ResolveObjIndex(fidx.vt, (int)texcoords.size());
					if (ti >= 0 && ti < (int)texcoords.size()) uv = texcoords[ti];
				}

				Vector3 n{ 0.0f, 1.0f, 0.0f };
				if (!normals.empty() && fidx.vn != -1 && fidx.vn != 0) {
					int ni = ResolveObjIndex(fidx.vn, (int)normals.size());
					if (ni >= 0 && ni < (int)normals.size()) n = normals[ni];
				} else {
					// vnが無いOBJの場合：とりあえず上向き（必要なら面法線計算に変えてOK）
					n = Normalize(n);
				}

				out.position = { p.x, p.y, p.z, 1.0f };
				out.texcoord = uv;
				out.normal = n;
				out.pad = 0.0f;
				return out;
				};

			// ★fan triangulation: (0, i, i+1)
			for (size_t i = 1; i + 1 < face.size(); ++i) {
				VertexData v0 = makeVertex(face[0]);
				VertexData v1 = makeVertex(face[i]);
				VertexData v2 = makeVertex(face[i + 1]);

				// 右手→左手変換で面の向きが反転しやすいので、
				// 必要なら v1 と v2 を入れ替えて winding を揃える
				// （カリングが逆ならここをONに）
				// std::swap(v1, v2);

				model.vertices.push_back(v0);
				model.vertices.push_back(v1);
				model.vertices.push_back(v2);
			}
		}
	}

	return model;
}

ObjModelData LoadObjFileAssimp(
	const std::string& directoryPath,
	const std::string& filename,
	bool useIndex // true: indexed / false: 三角形リスト
) {
	ObjModelData model{};

	Assimp::Importer importer;

	const unsigned int flags =
		aiProcess_Triangulate |
		aiProcess_FlipWindingOrder |
		aiProcess_FlipUVs;

	const std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
	assert(scene && scene->HasMeshes());

	// -----------------------------
	// material を解析（資料14）
	// 簡易版：Diffuse 1枚だけ採用
	// -----------------------------
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		assert(material);

		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			model.diffuseTexturePath = directoryPath + "/" + std::string(textureFilePath.C_Str());
			break;
		}
	}

	// -----------------------------
	// mesh / face / vertex を解析（資料10〜13）
	// -----------------------------
	if (!useIndex) {
		// ===== 非indexed：資料そのまま（三角形の頂点を3つpush）=====
		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			aiMesh* mesh = scene->mMeshes[meshIndex];
			assert(mesh);
			assert(mesh->HasNormals());
			assert(mesh->HasTextureCoords(0));

			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
				aiFace& face = mesh->mFaces[faceIndex];
				assert(face.mNumIndices == 3);

				for (uint32_t element = 0; element < face.mNumIndices; ++element) {
					uint32_t vertexIndex = face.mIndices[element];

					aiVector3D p = mesh->mVertices[vertexIndex];
					aiVector3D n = mesh->mNormals[vertexIndex];
					aiVector3D uv = mesh->mTextureCoords[0][vertexIndex];

					VertexData v{};
					v.position = { p.x, p.y, p.z, 1.0f };
					v.normal = { n.x, n.y, n.z };
					v.texcoord = { uv.x, uv.y };
					v.pad = 0.0f;

					// 左手座標系寄せ：Z反転（資料の例）
					v.position.z *= -1.0f;
					v.normal.z *= -1.0f;

					model.vertices.push_back(v);
				}
			}
		}
	} else {
		// ===== indexed：index buffer を作る =====
		// 方式：meshの “vertexIndex” をそのまま unique 化して VB を作り、FaceはIBで参照する
		// これが一番簡単で、資料の次ステップとして自然。

		// どの vertexIndex が、VBの何番になったか
		std::unordered_map<uint32_t, uint32_t> remap;
		remap.reserve(65536);

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			aiMesh* mesh = scene->mMeshes[meshIndex];
			assert(mesh);
			assert(mesh->HasNormals());
			assert(mesh->HasTextureCoords(0));

			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
				aiFace& face = mesh->mFaces[faceIndex];
				assert(face.mNumIndices == 3);

				for (uint32_t element = 0; element < 3; ++element) {
					uint32_t srcIndex = face.mIndices[element];

					auto it = remap.find(srcIndex);
					if (it == remap.end()) {
						aiVector3D p = mesh->mVertices[srcIndex];
						aiVector3D n = mesh->mNormals[srcIndex];
						aiVector3D uv = mesh->mTextureCoords[0][srcIndex];

						VertexData v{};
						v.position = { p.x, p.y, p.z, 1.0f };
						v.normal = { n.x, n.y, n.z };
						v.texcoord = { uv.x, uv.y };
						v.pad = 0.0f;

						// 左手座標系寄せ：Z反転（資料の例）
						v.position.z *= -1.0f;
						v.normal.z *= -1.0f;

						uint32_t newIndex = static_cast<uint32_t>(model.vertices.size());
						model.vertices.push_back(v);
						remap[srcIndex] = newIndex;
						model.indices.push_back(newIndex);
					} else {
						model.indices.push_back(it->second);
					}
				}
			}
		}
	}

	model.rootNode = ReadNode(scene->mRootNode);
	return model;
}

struct PointLight {
	Vector4 color;
	Vector3 position;
	float intensity;
	float radius;
	float decay;
	int32_t enable;     // ★追加
	float padding[1];   // ★16byte合わせ（合計で調整）
};

struct SpotLight {
	Vector4 color;
	Vector3 position;
	float intensity;

	Vector3 direction;
	float distance;

	float decay;
	float cosAngle;
	float cosFalloffStart; // ★追加（スライド通り）
	int32_t enable;        // ★追加
	float padding[1];      // ★16byte合わせ
};

struct RectLight {
	Vector4 color;        // RGB
	float intensity;      // 強度

	Vector3 position;     // 矩形の中心
	float  pad0;

	Vector3 normal;       // 矩形の表面法線（単位ベクトル）
	float  pad1;

	Vector3 tangent;      // 矩形の横方向（単位ベクトル）
	float  halfWidth;     // 半幅

	float  halfHeight;    // 半高さ
	int32_t enable;       // 0/1
	float pad2[2];        // 16byte合わせ
};


// 1. ConvertString関数
std::wstring ConvertString(const std::string& str) {
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wstr(len, 0);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], len);
	return wstr;
}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

// 2. Log関数
void Log(const std::wstring& message) {
	OutputDebugStringW(message.c_str());
	OutputDebugStringW(L"\n"); // 改行も出す
}



// DXGI_DEBUG系のGUID定義
EXTERN_C const GUID DECLSPEC_SELECTANY DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, { 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x08 } };
EXTERN_C const GUID DECLSPEC_SELECTANY DXGI_DEBUG_APP = { 0x25cddaa4, 0xb1c6, 0x47e1, { 0xac, 0x3e, 0x98, 0xb5, 0x4d, 0x0b, 0x64, 0x2d } };
EXTERN_C const GUID DECLSPEC_SELECTANY DXGI_DEBUG_D3D12 = { 0x6d2e06cf, 0x9646, 0x4b1f, { 0xa5, 0x7e, 0xdc, 0xe2, 0x60, 0x74, 0x6c, 0xf9 } };

// 画面サイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

// グローバル変数（各種DirectXオブジェクト）
ID3D12Device* device = nullptr;
IDXGIFactory7* dxgiFactory = nullptr;
ID3D12CommandQueue* commandQueue = nullptr;
ID3D12CommandAllocator* commandAllocator = nullptr;
ID3D12GraphicsCommandList* commandList = nullptr;
IDXGISwapChain4* swapChain = nullptr;
ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
ID3D12Resource* swapChainResources[2] = { nullptr, nullptr };
D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
ID3D12Fence* fence = nullptr;
UINT64 fenceValue = 0;
HANDLE fenceEvent = nullptr;

// ウィンドウプロシージャ（標準）
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true; // ImGuiが処理した場合はここで返す
	}

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// --- ウィンドウ作成 ---
	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"MyWindowClass";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	// クライアント領域サイズ調整
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウ生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,
		L"My DirectX12 App",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wrc.right - wrc.left, wrc.bottom - wrc.top,
		nullptr, nullptr, hInstance, nullptr
	);
	assert(hwnd);
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	// デバッグレイヤー有効化
	ID3D12Debug* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		ID3D12Debug1* debugController1 = nullptr;
		if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1)))) {
			debugController1->SetEnableGPUBasedValidation(TRUE);
			debugController1->Release();
		}
		debugController->Release();
	}
#endif

	// --- DirectX12初期化 ---
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));
	hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
	assert(SUCCEEDED(hr));

	// DescriptorSizeを保存しておく
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// RTV用のヒープでディスクリプタの数は２。
	ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// SRV用のヒープでディスクリプタの数は128.
	ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	// コマンドキュー作成
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	// コマンドアロケータ作成
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	// コマンドリスト作成
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	// スワップチェイン作成
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	IDXGISwapChain1* tempSwapChain = nullptr;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain);
	assert(SUCCEEDED(hr));
	hr = tempSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain));
	assert(SUCCEEDED(hr));
	tempSwapChain->Release();

	// ImGui初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device, swapChainDesc.BufferCount, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvDescriptorHeap, srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// コマンドリストクローズ
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// フェンス作成
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent != nullptr);

	// dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));
	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
	UploadTextureData(textureResource, mipImages);

	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2, mipImages2);

	DirectX::ScratchImage mipImages3 = LoadTexture("Resources/grass.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	ID3D12Resource* textureResource3 = CreateTextureResource(device, metadata3);
	UploadTextureData(textureResource3, mipImages3);

	//metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	// 先頭はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// SRVを作成
	device->CreateShaderResourceView(
		textureResource,
		&srvDesc,
		textureSrvHandleCPU);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	// SRVを作成
	device->CreateShaderResourceView(textureResource2, &srvDesc2, textureSrvHandleCPU2);

	// grass用 SRV desc
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata3.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	// SRV heap の index=3 に置く
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);

	device->CreateShaderResourceView(textureResource3, &srvDesc3, textureSrvHandleCPU3);


	// ディスクリプタヒープの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.BaseShaderRegister = 0; // t0 から
	descriptorRange.NumDescriptors = 1;
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 1. RootParameter作成（CBV b0）
	D3D12_ROOT_PARAMETER rootParameters[8] = {};

	// [0] Material（b0）→ PixelShader用
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// [1] Light（b1）→ PixelShader用
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	// [2] TransformationMatrix（b2）→ **VertexShader用**
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // ✅ ←重要
	rootParameters[2].Descriptor.ShaderRegister = 2; // ✅ b2 を使う想定なら b2に

	// [3] テクスチャ（t0）→ PixelShader用
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRange;

	// rootParameters[?] に追加
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].Descriptor.ShaderRegister = 3; // b3
	rootParameters[4].Descriptor.RegisterSpace = 0;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // or PIXEL

	// [5] PointLight（b4）
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 4;

	// [6] SpotLight（b5）
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].Descriptor.ShaderRegister = 5; // b5
	rootParameters[6].Descriptor.RegisterSpace = 0;


	// [7] RectLight（b6）
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[7].Descriptor.ShaderRegister = 6; // b6
	rootParameters[7].Descriptor.RegisterSpace = 0;


	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う

	// 2. RootSignatureの記述を構成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.NumStaticSamplers = 0;
	descriptionRootSignature.pStaticSamplers = nullptr;

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// 3. シリアライズ
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);
	if (FAILED(hr)) {
		Log(ConvertString(reinterpret_cast<char*>(errorBlob->GetBufferPointer())));
		assert(false);
	}

	// 4. CreateRootSignatureのために宣言が必要！
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr)) {
		if (errorBlob) {
			Log(ConvertString(reinterpret_cast<char*>(errorBlob->GetBufferPointer())));
		} else {
			Log(L"Unknown error in D3D12SerializeRootSignature");
		}
		assert(false);
	}

	// 球の頂点データを生成
	std::vector<VertexData> vertexDataSphere;
	const int kSubdivision = 32;

	for (int latitude = 0; latitude <= kSubdivision; ++latitude) {
		float theta = static_cast<float>(latitude) / kSubdivision * float(M_PI);
		for (int longitude = 0; longitude <= kSubdivision; ++longitude) {
			float phi = static_cast<float>(longitude) / kSubdivision * float(2.0 * M_PI);

			VertexData v{};
			v.position.x = sinf(theta) * cosf(phi);
			v.position.y = cosf(theta);
			v.position.z = sinf(theta) * sinf(phi);
			v.position.w = 1.0f;
			v.texcoord.x = static_cast<float>(longitude) / kSubdivision;
			v.texcoord.y = static_cast<float>(latitude) / kSubdivision;
			v.normal.x = v.position.x;
			v.normal.y = v.position.y;
			v.normal.z = v.position.z;

			vertexDataSphere.push_back(v);
		}
	}

	const float lonEvery = float(M_PI) * 2.0f / float(kSubdivision);
	const float latEvery = float(M_PI) / float(kSubdivision);

	auto calcPos = [](float theta, float phi) -> Vector4 {
		return {
			sinf(theta) * cosf(phi), // X
			cosf(theta),             // Y
			sinf(theta) * sinf(phi), // Z
			1.0f
		};
		};

	auto calcUV = [](float theta, float phi) -> Vector2 {
		return {
			phi / (2.0f * float(M_PI)),   // U
			theta / float(M_PI)          // V（北極=0.0, 南極=1.0）
		};
		};

	for (int lat = 0; lat < kSubdivision; ++lat) {
		float theta1 = lat * latEvery;
		float theta2 = (lat + 1) * latEvery;

		for (int lon = 0; lon < kSubdivision; ++lon) {
			float phi1 = lon * lonEvery;
			float phi2 = (lon + 1) * lonEvery;

			// 4頂点作成
			Vector4 pA = calcPos(theta1, phi1); Vector2 uvA = calcUV(theta1, phi1);
			Vector4 pB = calcPos(theta2, phi1); Vector2 uvB = calcUV(theta2, phi1);
			Vector4 pC = calcPos(theta1, phi2); Vector2 uvC = calcUV(theta1, phi2);
			Vector4 pD = calcPos(theta2, phi2); Vector2 uvD = calcUV(theta2, phi2);

			auto calcNormal = [](const Vector4& p) -> Vector3 {
				return Normalize(Vector3{ p.x, p.y, p.z });
				};

			// 三角形1（A→C→B）
			vertexDataSphere.push_back({ pA, uvA, calcNormal(pA), 0.0f });
			vertexDataSphere.push_back({ pC, uvC, calcNormal(pC), 0.0f });
			vertexDataSphere.push_back({ pB, uvB, calcNormal(pB), 0.0f });

			// 三角形2（C→D→B）
			vertexDataSphere.push_back({ pC, uvC, calcNormal(pC), 0.0f });
			vertexDataSphere.push_back({ pD, uvD, calcNormal(pD), 0.0f });
			vertexDataSphere.push_back({ pB, uvB, calcNormal(pB), 0.0f });


		}
	}

	// リソース作成
	// vertexDataSphere.size() に合わせて確保！
	size_t vertexBufferSize = sizeof(VertexData) * vertexDataSphere.size();
	ID3D12Resource* vertexResource = CreateBufferResource(device, vertexBufferSize);

	ID3D12Resource* cameraResource = CreateBufferResource(device, sizeof(CameraForGPU));
	CameraForGPU* cameraData = nullptr;
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));


	// --- Sprite用のリソースとビューを作成 ---
	ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);
	ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// ===== OBJ 用 =====
	ID3D12Resource* vertexResourceObj = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewObj{};
	UINT objVertexCount = 0;

	// ★追加
	UINT objIndexCount = 0;
	ID3D12Resource* indexResourceObj = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewObj{};

	// ===== Terrain 用（★追加）=====
	ID3D12Resource* vertexResourceTerrain = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewTerrain{};
	UINT terrainVertexCount = 0;

	UINT terrainIndexCount = 0;
	ID3D12Resource* indexResourceTerrain = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewTerrain{};

	// ★Terrain用 TransformCB（b2差し替え用）
	ID3D12Resource* wvpResourceTerrain = nullptr;
	TransformationMatrix* wvpDataTerrain = nullptr;


	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; 
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

	// 左上
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[0].normal = { 0.0f, 0.0f, -1.0f };

	// 左下
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[1].normal = { 0.0f, 0.0f, -1.0f };

	// 右上
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[2].normal = { 0.0f, 0.0f, -1.0f };

	// 右下
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[3].normal = { 0.0f, 0.0f, -1.0f };


	// Sprite用マテリアルリソースを作成
	ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataSprite->enableLighting = false;


	// Sprite用のTransformationMatrix用のリソースを作る
	ID3D12Resource* transformationMatrixResourceSprite =
		CreateBufferResource(device, sizeof(TransformationMatrix));

	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	transformationMatrixResourceSprite->Map(0, nullptr,
		reinterpret_cast<void**>(&transformationMatrixDataSprite));

	// マテリアル用のリソースを作る
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));

	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = 1;
	materialData->shininess = 32.0f;   // 例：スライド通り “光沢度”


	auto Align256 = [](size_t s) { return (s + 0xFF) & ~0xFF; };
	const size_t kWvpSize = Align256(sizeof(TransformationMatrix));

	// ★Sphere用
	ID3D12Resource* wvpResourceSphere = CreateBufferResource(device, kWvpSize);
	TransformationMatrix* wvpDataSphere = nullptr;
	wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataSphere));

	// ★OBJ用
	ID3D12Resource* wvpResourceObj = CreateBufferResource(device, kWvpSize);
	TransformationMatrix* wvpDataObj = nullptr;
	wvpResourceObj->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataObj));

	// ★Terrain用
	wvpResourceTerrain = CreateBufferResource(device, kWvpSize);
	wvpResourceTerrain->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataTerrain));

	wvpDataTerrain->WVP = MakeIdentity4x4();
	wvpDataTerrain->World = MakeIdentity4x4();
	wvpDataTerrain->WorldInverseTranspose = MakeIdentity4x4();


	wvpDataSphere->WVP = MakeIdentity4x4();
	wvpDataSphere->World = MakeIdentity4x4();
	wvpDataSphere->WorldInverseTranspose = MakeIdentity4x4();

	wvpDataObj->WVP = MakeIdentity4x4();
	wvpDataObj->World = MakeIdentity4x4();
	wvpDataObj->WorldInverseTranspose = MakeIdentity4x4();


	// ライト用の定数バッファリソースを作成
	ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	// 書き込み用ポインタの定義（これが必要）
	DirectionalLight* directionalLightData = nullptr;
	// マップしてアドレス取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// 初期化（単位ベクトルで）
	directionalLightData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	directionalLightData->direction = Vector3(0.0f, -1.0f, 0.0f); // 正規化されてること
	directionalLightData->intensity = 1.0f;

	// ===== PointLight =====
	ID3D12Resource* pointLightResource =
		CreateBufferResource(device, sizeof(PointLight));

	PointLight* pointLightData = nullptr;
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));

	// 初期値（好みで）
	pointLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData->position = { 0.0f, 1.0f, 10.0f }; // オブジェクトの近く
	pointLightData->radius = 6.0f;
	pointLightData->decay = 2.0f;
	pointLightData->intensity = 2.0f;

	pointLightData->padding[0] = 0.0f;
	pointLightData->padding[1] = 0.0f;



	// ===== SpotLight =====
	ID3D12Resource* spotLightResource =
		CreateBufferResource(device, sizeof(SpotLight));

	SpotLight* spotLightData = nullptr;
	spotLightResource->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData));

	// 初期値（例）
	spotLightData->color = { 1, 1, 1, 1 };
	spotLightData->position = { 5.0f, 1.0f, 10.0f };
	spotLightData->intensity = 4.0f;

	// 方向（左手系なら「+Zが前」想定のままでOK）
	spotLightData->direction = Normalize({ -1.0f, -1.0f, 0.0f }); // 斜め前下など
	spotLightData->distance = 50.0f;
	spotLightData->decay = 2.0f;

	// cosAngle：例えば 30度なら cos(30°)
	spotLightData->cosAngle = cosf(30.0f * float(M_PI) / 180.0f);

	float outerDeg = 30.0f;
	float innerDeg = 20.0f;

	spotLightData->cosAngle = cosf(outerDeg * float(M_PI) / 180.0f);
	spotLightData->cosFalloffStart = cosf(innerDeg * float(M_PI) / 180.0f);

	
	pointLightData->enable = 1;
	spotLightData->enable = 1;

	// ===== RectLight =====
	ID3D12Resource* rectLightResource =
		CreateBufferResource(device, sizeof(RectLight));

	RectLight* rectLightData = nullptr;
	rectLightResource->Map(0, nullptr, reinterpret_cast<void**>(&rectLightData));

	// 初期値（例）
	rectLightData->color = { 1,1,1,1 };
	rectLightData->intensity = 6.0f;
	rectLightData->position = { 0.0f, 3.0f, 8.0f };

	// 左手座標系で +Z 前。例えば「下向きに照らす」なら normal は (0,-1,0)
	rectLightData->normal = Normalize({ 0.0f, -1.0f, 0.0f });

	// tangent は normal と直交する方向（横方向）
	rectLightData->tangent = Normalize({ 1.0f,  0.0f, 0.0f });

	rectLightData->halfWidth = 2.0f;
	rectLightData->halfHeight = 1.0f;

	rectLightData->enable = 1;


	// インプットレイアウト
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};

	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	inputElementDescs[0].InputSlot = 0;
	inputElementDescs[0].InstanceDataStepRate = 0;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	inputElementDescs[1].InputSlot = 0;
	inputElementDescs[1].InstanceDataStepRate = 0;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].InputSlot = 0;
	inputElementDescs[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	inputElementDescs[2].InstanceDataStepRate = 0;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// ブレンドステイト
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	D3D12_COLOR_WRITE_ENABLE_ALL;

	// ラスタライザーステイト
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);
	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);

	// PSOを生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature;
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLess Equal。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 利用するトロポジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));


	// 頂点バッファビューを作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースサイズは頂点3つ分のサイズ
	// 球のデータサイズに合わせる
	vertexBufferView.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(vertexDataSphere.size());

	// 1つの頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// ===== OBJ 読み込み → VB 作成 =====
	//ObjModelData obj = LoadObjFile("Resources/terrain.obj"); // ここ好きなOBJへ
	/*objVertexCount = (UINT)obj.vertices.size();
	assert(objVertexCount > 0);*/

	// ===== OBJ 読み込み → VB/IB 作成 =====
	bool useIndex = true; // indexedで描くならtrue

	ObjModelData obj = LoadObjFileAssimp("Resources", "plane.gltf", useIndex);
	Log(ConvertString("OBJ diffuseTexturePath: " + obj.diffuseTexturePath));

	// ★ここが抜けてた：カウントを代入
	objVertexCount = (UINT)obj.vertices.size();
	assert(objVertexCount > 0);

	if (useIndex) {
		objIndexCount = (UINT)obj.indices.size();
		assert(objIndexCount > 0);
	}

	// ===== Terrain 読み込み（★追加）=====
	ObjModelData terrain = LoadObjFileAssimp("Resources", "terrain.obj", useIndex);
	terrainVertexCount = (UINT)terrain.vertices.size();
	assert(terrainVertexCount > 0);
	if (useIndex) {
		terrainIndexCount = (UINT)terrain.indices.size();
		assert(terrainIndexCount > 0);
	}
	// --- OBJ VertexBuffer ---
	//vertexResourceObj = CreateBufferResource(device, sizeof(VertexData) * objVertexCount);

	//vertexBufferViewObj.BufferLocation = vertexResourceObj->GetGPUVirtualAddress();
	//vertexBufferViewObj.SizeInBytes = (UINT)(sizeof(VertexData) * objVertexCount);
	//vertexBufferViewObj.StrideInBytes = sizeof(VertexData);

	//// VBへコピー
	//{
	//	VertexData* mappedObj = nullptr;
	//	HRESULT hr2 = vertexResourceObj->Map(0, nullptr, reinterpret_cast<void**>(&mappedObj));
	//	assert(SUCCEEDED(hr2));
	//	memcpy(mappedObj, obj.vertices.data(), sizeof(VertexData) * objVertexCount);
	//	vertexResourceObj->Unmap(0, nullptr);
	//}
	if (useIndex) {
		indexResourceObj = CreateBufferResource(device, sizeof(uint32_t) * objIndexCount);

		uint32_t* indexDataObj = nullptr;
		HRESULT hrOI = indexResourceObj->Map(0, nullptr, reinterpret_cast<void**>(&indexDataObj));
		assert(SUCCEEDED(hrOI));
		memcpy(indexDataObj, obj.indices.data(), sizeof(uint32_t) * objIndexCount);
		indexResourceObj->Unmap(0, nullptr);

		indexBufferViewObj.BufferLocation = indexResourceObj->GetGPUVirtualAddress();
		indexBufferViewObj.SizeInBytes = (UINT)(sizeof(uint32_t) * objIndexCount);
		indexBufferViewObj.Format = DXGI_FORMAT_R32_UINT;
	}

	// ===== Terrain Vertex/IndexBuffer（★追加）=====
	vertexResourceTerrain = CreateBufferResource(device, sizeof(VertexData) * terrainVertexCount);
	vertexBufferViewTerrain.BufferLocation = vertexResourceTerrain->GetGPUVirtualAddress();
	vertexBufferViewTerrain.SizeInBytes = (UINT)(sizeof(VertexData) * terrainVertexCount);
	vertexBufferViewTerrain.StrideInBytes = sizeof(VertexData);

	{
		VertexData* mappedTerrain = nullptr;
		HRESULT hrT = vertexResourceTerrain->Map(0, nullptr, reinterpret_cast<void**>(&mappedTerrain));
		assert(SUCCEEDED(hrT));
		memcpy(mappedTerrain, terrain.vertices.data(), sizeof(VertexData) * terrainVertexCount);
		vertexResourceTerrain->Unmap(0, nullptr);
	}

	// --- OBJ IndexBuffer（indexedのときだけ）---
	if (useIndex) {
		indexResourceTerrain = CreateBufferResource(device, sizeof(uint32_t) * terrainIndexCount);
		uint32_t* indexDataTerrain = nullptr;
		HRESULT hrTI = indexResourceTerrain->Map(0, nullptr, reinterpret_cast<void**>(&indexDataTerrain));
		assert(SUCCEEDED(hrTI));
		memcpy(indexDataTerrain, terrain.indices.data(), sizeof(uint32_t) * terrainIndexCount);
		indexResourceTerrain->Unmap(0, nullptr);

		indexBufferViewTerrain.BufferLocation = indexResourceTerrain->GetGPUVirtualAddress();
		indexBufferViewTerrain.SizeInBytes = (UINT)(sizeof(uint32_t) * terrainIndexCount);
		indexBufferViewTerrain.Format = DXGI_FORMAT_R32_UINT;
	}
	/*uint32_t* indexData = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	memcpy(indexData, obj.indices.data(), sizeof(uint32_t) * obj.indices.size());
	indexResource->Unmap(0, nullptr);*/

	/*D3D12_INDEX_BUFFER_VIEW ibv{};
	ibv.BufferLocation = indexResource->GetGPUVirtualAddress();
	ibv.SizeInBytes = (UINT)(sizeof(uint32_t) * obj.indices.size());
	ibv.Format = DXGI_FORMAT_R32_UINT;*/

	// OBJ頂点バッファ
	vertexResourceObj = CreateBufferResource(device, sizeof(VertexData) * objVertexCount);

	// View
	vertexBufferViewObj.BufferLocation = vertexResourceObj->GetGPUVirtualAddress();
	vertexBufferViewObj.SizeInBytes = (UINT)(sizeof(VertexData) * objVertexCount);
	vertexBufferViewObj.StrideInBytes = sizeof(VertexData);

	// コピー
	VertexData* mappedObj = nullptr;
	HRESULT hr2 = vertexResourceObj->Map(0, nullptr, reinterpret_cast<void**>(&mappedObj));
	assert(SUCCEEDED(hr2));
	memcpy(mappedObj, obj.vertices.data(), sizeof(VertexData)* objVertexCount);
	vertexResourceObj->Unmap(0, nullptr);


	VertexData* vertexData = nullptr;

	// Map の結果チェック
	hr = vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	assert(SUCCEEDED(hr));
	assert(vertexData != nullptr);

	// サイズの整合性確認
	size_t dataSize = sizeof(VertexData) * vertexDataSphere.size();
	assert(dataSize > 0); // 0バイトだとそもそもダメ
	assert(vertexResource != nullptr);

	// memcpy の前に resource サイズが十分か確認
	D3D12_RESOURCE_DESC desc = vertexResource->GetDesc();
	assert(desc.Width >= dataSize);

	// コピー処理
	memcpy(vertexData, vertexDataSphere.data(), dataSize);

	// Unmap は Uploadヒープでは通常不要だが、デバッグ用に
	vertexResource->Unmap(0, nullptr);


	// ビューポート
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// RTV用ディスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 2;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	assert(SUCCEEDED(hr));

	// バックバッファ取得＆RTV作成
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < 2; ++i) {
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
		assert(SUCCEEDED(hr));
		rtvHandles[i] = GetCPUDescriptorHandle(rtvDescriptorHeap, descriptorSizeRTV, i);
		device->CreateRenderTargetView(swapChainResources[i], &rtvDesc, rtvHandles[i]);
	}


	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DepthStencil Textureをウィンドウのサイズで作成
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
	// DSVHeapの先頭にDSVをつくる
	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	bool useMonsterBall = true;

	static bool drawSphere = true;
	static bool drawObj = true;
	static bool drawSprite = true;
	static bool drawTerrain = true;

	static bool enableDirectional = true;
	static bool enablePoint = true;
	static bool enableSpot = true;


	// --- メインループ ---
	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {


			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator, graphicsPipelineState);
			assert(SUCCEEDED(hr));

			// Transform変数を作る
			static Transform transform = {
				  {1.0f, 1.0f, 1.0f},  // scale
				  {0.0f, 0.0f, 0.0f},  // rotate
				  {0.0f, -2.0f, 10.0f}   // translate
			};

			static Transform cameraTransform = {
				  {1.0f, 1.0f, 1.0f},  // scale
				  {0.1f, 0.0f, 0.0f},  // rotate
				  {0.0f, 0.0f, -5.0f}   // translate
			};

			// Sphere用
			static Transform transformSphere = {
			  {1.0f, 1.0f, 1.0f},
			  {0.0f, 0.0f, 0.0f},
			  {0.0f, -2.0f, 10.0f}
			};

			// ★OBJ用（別物）
			static Transform transformObj = {
			  {1.0f, 1.0f, 1.0f},
			  {3.14f, 0.0f, 0.0f},
			  {0.0f, -2.0f, 10.0f}
			};

			// ★Terrain用（別物）
			static Transform transformTerrain = {
			  {1.0f, 1.0f, 1.0f},
			  {0.0f, 0.0f, 0.0f},
			  {0.0f, -2.0f, 10.0f}
			};

			Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
				0.45f,
				float(kClientWidth) / float(kClientHeight),
				0.1f, 100.0f
			);

			// Sphere用
			Matrix4x4 worldSphere = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
			wvpDataSphere->World = worldSphere;
			wvpDataSphere->WVP = Multiply(worldSphere, Multiply(viewMatrix, projectionMatrix));
			wvpDataSphere->WorldInverseTranspose = Transpose(Inverse(worldSphere));

			// Obj用
			Matrix4x4 worldObj = MakeAffineMatrix(transformObj.scale, transformObj.rotate, transformObj.translate);
			Matrix4x4 worldObjWithRoot = Multiply(obj.rootNode.localMatrix, worldObj);

			wvpDataObj->World = worldObjWithRoot;
			wvpDataObj->WVP = Multiply(worldObj, Multiply(viewMatrix, projectionMatrix));
			wvpDataObj->WorldInverseTranspose = Transpose(Inverse(worldObj));


			cameraData->worldPosition = cameraTransform.translate;
			cameraData->padding = 0.0f;


			// Sprite用のTransform
			static Transform transformSprite = {
				{1.0f, 1.0f, 1.0f},  // scale
				{0.0f, 0.0f, 0.0f},  // rotate
				{0.0f, 1280.0f, 0.0f}   // translate
			};

			// Sprite用のWVP行列を作成（正射影）
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4(); // スプライトはビュー不要
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

			transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;


			commandList->SetGraphicsRootSignature(rootSignature);
			// マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
			static float dirDefaultIntensity = 1.0f;
			directionalLightData->intensity = enableDirectional ? directionalLightData->intensity : 0.0f; // これは雑なので下のPS方式推奨
			pointLightData->enable = enablePoint ? 1 : 0;
			spotLightData->enable = enableSpot ? 1 : 0;


			// Present -> RenderTargetに遷移
			D3D12_RESOURCE_BARRIER barrierBegin{};
			barrierBegin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrierBegin.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrierBegin.Transition.pResource = swapChainResources[backBufferIndex];
			barrierBegin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrierBegin.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrierBegin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			commandList->ResourceBarrier(1, &barrierBegin);

			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			// 指定した深度で画面全体をクリアする	
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// DrawCall
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);

			commandList->SetGraphicsRootSignature(rootSignature);

			// b0 material
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			// b1 light
			commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());

			// b3 camera（rootIndexはあなたのRootSignatureに合わせて）
			commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

			// b4 PointLight
			commandList->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());

			// b5 SpotLight
			commandList->SetGraphicsRootConstantBufferView(6, spotLightResource->GetGPUVirtualAddress());

			// b6 RectLight
			commandList->SetGraphicsRootConstantBufferView(7, rectLightResource->GetGPUVirtualAddress());


			// SRV heap + t0
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// draw
			commandList->SetPipelineState(graphicsPipelineState);

			if (drawSphere) {
				Matrix4x4 worldSphere =
					MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);

				// Sphere は自前生成なので RootNode は掛けない
				wvpDataSphere->World = worldSphere;
				wvpDataSphere->WVP = Multiply(worldSphere, Multiply(viewMatrix, projectionMatrix));
				wvpDataSphere->WorldInverseTranspose = Transpose(Inverse(worldSphere));

				// b2 transform(WVP/World) を Sphere用に差し替え
				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceSphere->GetGPUVirtualAddress());

				// t0
				commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandleGPU2);

				// draw
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->DrawInstanced((UINT)vertexDataSphere.size(), 1, 0, 0);
			}

			if (drawObj) {
				Matrix4x4 worldObj =
					MakeAffineMatrix(transformObj.scale, transformObj.rotate, transformObj.translate);

				// ★ glTF/assimp の RootNode 行列を適用
				Matrix4x4 worldObjWithRoot = Multiply(obj.rootNode.localMatrix, worldObj);

				wvpDataObj->World = worldObjWithRoot;
				wvpDataObj->WVP = Multiply(worldObjWithRoot, Multiply(viewMatrix, projectionMatrix));
				wvpDataObj->WorldInverseTranspose = Transpose(Inverse(worldObjWithRoot));

				// b2 transform(WVP/World) を Obj用に差し替え
				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceObj->GetGPUVirtualAddress());

				// t0
				commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandleGPU);

				// draw
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewObj);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				if (useIndex) {
					commandList->IASetIndexBuffer(&indexBufferViewObj);
					commandList->DrawIndexedInstanced(objIndexCount, 1, 0, 0, 0);
				} else {
					commandList->DrawInstanced(objVertexCount, 1, 0, 0);
				}
			}



			if (drawTerrain) {
				Matrix4x4 worldTerrain =
					MakeAffineMatrix(transformTerrain.scale, transformTerrain.rotate, transformTerrain.translate);

				// assimp rootNode 行列も同じように適用（glTF/OBJどっちでも）
				Matrix4x4 worldTerrainWithRoot = Multiply(terrain.rootNode.localMatrix, worldTerrain);

				wvpDataTerrain->World = worldTerrainWithRoot;
				wvpDataTerrain->WVP = Multiply(worldTerrainWithRoot, Multiply(viewMatrix, projectionMatrix));
				wvpDataTerrain->WorldInverseTranspose = Transpose(Inverse(worldTerrainWithRoot));

				// b2 を terrain用に差し替え
				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceTerrain->GetGPUVirtualAddress());

				// とりあえず uvChecker を使う（専用テクスチャにしたいなら後述）
				commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandleGPU3);

				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewTerrain);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				if (useIndex) {
					commandList->IASetIndexBuffer(&indexBufferViewTerrain);
					commandList->DrawIndexedInstanced(terrainIndexCount, 1, 0, 0, 0);
				} else {
					commandList->DrawInstanced(terrainVertexCount, 1, 0, 0);
				}
			}



			// Spriteの描画
			//commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			//commandList->IASetIndexBuffer(&indexBufferViewSprite);

			//// マテリアルをSprite用に切り替え
			//commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
			//// TransformationMatrixCBufferの場所を設定
			//commandList->SetGraphicsRootConstantBufferView(2, transformationMatrixResourceSprite->GetGPUVirtualAddress());

			//commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandleGPU);
			//commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

			//commandList -> DrawIndexedInstanced(6, 1, 0, 0, 0); // Spriteの描画

			//描画
			

			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// 自作ウィンドウだけ表示する
			ImGui::Begin("Model");
			ImGui::Separator();
			ImGui::Text("Render Toggle");
			ImGui::Checkbox("Draw Sphere", &drawSphere);
			ImGui::Checkbox("Draw Terrain", &drawTerrain);
			ImGui::Checkbox("Draw Plane.gltf", &drawObj);
			/*ImGui::SliderFloat3("Translate", &transformSprite.translate.x, 0.0f, 1280.0f);
			ImGui::SliderFloat3("Scale", &transformSprite.scale.x, 0.0f, 5.0f);
			ImGui::SliderFloat3("Rotate", &transformSprite.rotate.x, -3.14f, 3.14f);*/
			ImGui::Separator();
			ImGui::Text("Camera Controls");
			ImGui::SliderFloat3("Camera Position", &cameraTransform.translate.x, -10.0f, 10.0f);
			ImGui::SliderFloat3("Camera Rotation", &cameraTransform.rotate.x, -3.14f, 3.14f);

			// ↓ 以下を追加
			ImGui::Separator();
			ImGui::Text("Sphere Controls");
			ImGui::SliderFloat3("Sphere Translate", &transformSphere.translate.x, -10.0f, 10.0f);
			ImGui::SliderFloat3("Sphere Rotate", &transformSphere.rotate.x, -3.14f, 3.14f);
			ImGui::SliderFloat3("Sphere Scale", &transformSphere.scale.x, 0.0f, 5.0f);

			ImGui::Separator();
			ImGui::Text("Plane Controls");
			ImGui::SliderFloat3("Plane Translate", &transformObj.translate.x, -10.0f, 10.0f);
			ImGui::SliderFloat3("Plane Rotate", &transformObj.rotate.x, -3.14f, 3.14f);
			ImGui::SliderFloat3("Plane Scale", &transformObj.scale.x, 0.0f, 5.0f);

			ImGui::Separator();
			ImGui::Text("Terrain Controls");
			ImGui::SliderFloat3("Terrain Translate", &transformTerrain.translate.x, -50.0f, 50.0f);
			ImGui::SliderFloat3("Terrain Rotate", &transformTerrain.rotate.x, -3.14f, 3.14f);
			ImGui::SliderFloat3("Terrain Scale", &transformTerrain.scale.x, 0.0f, 20.0f);


			//ImGui::Checkbox("useMonsterBall", &useMonsterBall);

			ImGui::End();
			static bool enableRect = true;
			rectLightData->enable = enableRect ? 1 : 0;
			ImGui::Begin("Light");
			ImGui::Separator();
			ImGui::Text("Light Toggle");
			ImGui::Checkbox("Directional", &enableDirectional);
			ImGui::Checkbox("Point", &enablePoint);
			ImGui::Checkbox("Spot", &enableSpot);
			ImGui::Checkbox("Rect Enable", &enableRect);
			// -------- DirectionalLight (あるなら) --------
			// ImGui::SliderFloat3(...)
			ImGui::SeparatorText("DirectionalLight");
			ImGui::ColorEdit3("Material Color", reinterpret_cast<float*>(&materialData->color));
			ImGui::Checkbox("Enable Lighting", reinterpret_cast<bool*>(&materialData->enableLighting));

			ImGui::ColorEdit3("Light Color", reinterpret_cast<float*>(&directionalLightData->color));
			ImGui::SliderFloat3("Light Dir", reinterpret_cast<float*>(&directionalLightData->direction), -1.0f, 1.0f);
			ImGui::SliderFloat("Intensity", &directionalLightData->intensity, 0.0f, 5.0f);

			// -------- PointLight --------
			ImGui::SeparatorText("PointLight");

			// 位置
			ImGui::DragFloat3("PL Position", &pointLightData->position.x, 0.01f, -50.0f, 50.0f);

			// 色（ColorEdit3はRGBだけ。alpha触りたいならColorEdit4）
			ImGui::ColorEdit3("PL Color", &pointLightData->color.x);

			// 強度
			ImGui::DragFloat("PL Intensity", &pointLightData->intensity, 0.01f, 0.0f, 10.0f);

			// 届く範囲
			ImGui::DragFloat("PL Radius", &pointLightData->radius, 0.01f, 0.01f, 10.0f);

			// 減衰率（大きいほど急激）
			ImGui::DragFloat("PL Decay", &pointLightData->decay, 0.01f, 0.01f, 10.0f);

			// 便利：球をライト位置に追従させたいなら（任意）
			// if (ImGui::Button("Move Sphere To Light")) { sphereTransform.translate = pointLightData->position; }
			

			ImGui::SeparatorText("SpotLight");

			// Position
			ImGui::DragFloat3("SL Position", &spotLightData->position.x, 0.01f, -50.0f, 50.0f);

			// Direction（正規化するのが大事）
			ImGui::DragFloat3("SL Direction", &spotLightData->direction.x, 0.01f, -1.0f, 1.0f);
			spotLightData->direction = Normalize(spotLightData->direction);

			// Color
			ImGui::ColorEdit3("SL Color", &spotLightData->color.x);

			// Intensity / Distance / Decay
			ImGui::DragFloat("SL Intensity", &spotLightData->intensity, 0.01f, 0.0f, 20.0f);
			ImGui::DragFloat("SL Distance", &spotLightData->distance, 0.01f, 0.01f, 200.0f);
			ImGui::DragFloat("SL Decay", &spotLightData->decay, 0.01f, 0.01f, 10.0f);

			// Angle（度で触ってcosに変換すると扱いやすい）
			static float slAngleDeg = 30.0f;
			ImGui::SliderFloat("SL Angle(deg)", &slAngleDeg, 1.0f, 89.0f);
			spotLightData->cosAngle = cosf(slAngleDeg * float(M_PI) / 180.0f);

			static float slOuterDeg = 30.0f; // 外側
			static float slInnerDeg = 20.0f; // 内側

			ImGui::SliderFloat("SL Outer (deg)", &slOuterDeg, 1.0f, 89.0f);
			ImGui::SliderFloat("SL Inner (deg)", &slInnerDeg, 0.0f, 88.0f);

			// inner は outer より小さく（内側なので）
			if (slInnerDeg >= slOuterDeg) slInnerDeg = slOuterDeg - 0.1f;

			spotLightData->cosAngle = cosf(slOuterDeg * float(M_PI) / 180.0f);
			spotLightData->cosFalloffStart = cosf(slInnerDeg * float(M_PI) / 180.0f);

			ImGui::SeparatorText("RectLight (Area)");
			

			ImGui::DragFloat3("RL Position", &rectLightData->position.x, 0.01f, -50.0f, 50.0f);
			ImGui::DragFloat3("RL Normal", &rectLightData->normal.x, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat3("RL Tangent", &rectLightData->tangent.x, 0.01f, -1.0f, 1.0f);

			rectLightData->normal = Normalize(rectLightData->normal);
			rectLightData->tangent = Normalize(rectLightData->tangent);

			ImGui::ColorEdit3("RL Color", &rectLightData->color.x);
			ImGui::DragFloat("RL Intensity", &rectLightData->intensity, 0.01f, 0.0f, 50.0f);
			
			ImGui::End();


			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);



			// RenderTarget -> Presentに遷移
			D3D12_RESOURCE_BARRIER barrierEnd{};
			barrierEnd.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrierEnd.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrierEnd.Transition.pResource = swapChainResources[backBufferIndex];
			barrierEnd.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrierEnd.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			barrierEnd.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			commandList->ResourceBarrier(1, &barrierEnd);

			hr = commandList->Close();
			assert(SUCCEEDED(hr));
			ID3D12CommandList* cmdLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, cmdLists);
			swapChain->Present(1, 0);

			// フェンス同期
			fenceValue++;
			hr = commandQueue->Signal(fence, fenceValue);
			assert(SUCCEEDED(hr));
			if (fence->GetCompletedValue() < fenceValue) {
				hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
				assert(SUCCEEDED(hr));
				WaitForSingleObject(fenceEvent, INFINITE);
			}
		}
	}

	// --- 後片付け ---
	CloseHandle(fenceEvent);
	if (fence) fence->Release();
	for (int i = 0; i < 2; ++i) {
		if (swapChainResources[i]) swapChainResources[i]->Release();
	}
	if (rtvDescriptorHeap) rtvDescriptorHeap->Release();
	if (swapChain) swapChain->Release();
	if (commandList) commandList->Release();
	if (commandAllocator) commandAllocator->Release();
	if (commandQueue) commandQueue->Release();
	if (device) device->Release();
	if (dxgiFactory) dxgiFactory->Release();

	if (vertexResource) vertexResource->Release();
	if (graphicsPipelineState) graphicsPipelineState->Release();
	if (rootSignature) rootSignature->Release();
	if (vertexShaderBlob) vertexShaderBlob->Release();
	if (pixelShaderBlob) pixelShaderBlob->Release();
	if (signatureBlob) signatureBlob->Release();
	if (errorBlob) errorBlob->Release();
	materialResource->Release();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// リソース全開放後、LiveObjectsレポート
	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	CoUninitialize();

	return 0;
}

IDxcBlob* CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compilerに使用するProfile
	const wchar_t* profile,
	// 初期化して生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	// hlslファイルを読む
	Log(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile));
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	// Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E",L"main",
		L"-T",profile,
		L"-Zi",L"Qembed_debug",
		L"-Od",
		L"-Zpr",
	};
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr));

	// 警告エラーが出ていないか確認する
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(ConvertString(shaderError->GetStringPointer()));
		assert(false); // 本当にエラーなら止め
	}

	// Compile結果を取得する
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功したログを出す
	Log((std::format(L"Compile Succeeded,path:{},profile:{}\n", filePath, profile)));
	// もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* resource = nullptr;

	// リソース作成の前にログを出力
	Log(L"Creating material resource...");

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	// 作成に失敗した場合はエラーログを出力
	if (FAILED(hr)) {
		Log(L"Failed to create material resource");
	} else {
		Log(L"Material resource created successfully");
	}

	assert(SUCCEEDED(hr));
	return resource;
}
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result = {};

	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;

	return result;
}
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{
	Matrix4x4 matrix = {};
	float cosX = cosf(rotate.x);
	float sinX = sinf(rotate.x);
	float cosY = cosf(rotate.y);
	float sinY = sinf(rotate.y);
	float cosZ = cosf(rotate.z);
	float sinZ = sinf(rotate.z);
	matrix.m[0][0] = scale.x * (cosY * cosZ);
	matrix.m[0][1] = scale.x * (cosY * sinZ);
	matrix.m[0][2] = scale.x * (-sinY);
	matrix.m[0][3] = 0.0f;
	matrix.m[1][0] = scale.y * (sinX * sinY * cosZ - cosX * sinZ);
	matrix.m[1][1] = scale.y * (sinX * sinY * sinZ + cosX * cosZ);
	matrix.m[1][2] = scale.y * (sinX * cosY);
	matrix.m[1][3] = 0.0f;
	matrix.m[2][0] = scale.z * (cosX * sinY * cosZ + sinX * sinZ);
	matrix.m[2][1] = scale.z * (cosX * sinY * sinZ - sinX * cosZ);
	matrix.m[2][2] = scale.z * (cosX * cosY);
	matrix.m[2][3] = 0.0f;
	matrix.m[3][0] = translate.x;
	matrix.m[3][1] = translate.y;
	matrix.m[3][2] = translate.z;
	matrix.m[3][3] = 1.0f;
	return matrix;
}
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearZ, float farZ) {
	Matrix4x4 m{};
	float yScale = 1.0f / tanf(fovY / 2.0f);
	float xScale = yScale / aspect;
	float range = farZ - nearZ;

	m.m[0][0] = xScale;
	m.m[1][1] = yScale;
	m.m[2][2] = farZ / range;
	m.m[2][3] = 1.0f;
	m.m[3][2] = -nearZ * farZ / range;

	return m;
}
Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
	Matrix4x4 r{};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			for (int k = 0; k < 4; ++k) {
				r.m[row][col] += a.m[row][k] * b.m[k][col];
			}
		}
	}
	return r;
}
Matrix4x4 Inverse(const Matrix4x4& m)
{
	Matrix4x4 result;
	float* inv = &result.m[0][0];
	const float* mat = &m.m[0][0];

	float invOut[16];

	invOut[0] = mat[5] * mat[10] * mat[15] -
		mat[5] * mat[11] * mat[14] -
		mat[9] * mat[6] * mat[15] +
		mat[9] * mat[7] * mat[14] +
		mat[13] * mat[6] * mat[11] -
		mat[13] * mat[7] * mat[10];

	invOut[1] = -mat[1] * mat[10] * mat[15] +
		mat[1] * mat[11] * mat[14] +
		mat[9] * mat[2] * mat[15] -
		mat[9] * mat[3] * mat[14] -
		mat[13] * mat[2] * mat[11] +
		mat[13] * mat[3] * mat[10];

	invOut[2] = mat[1] * mat[6] * mat[15] -
		mat[1] * mat[7] * mat[14] -
		mat[5] * mat[2] * mat[15] +
		mat[5] * mat[3] * mat[14] +
		mat[13] * mat[2] * mat[7] -
		mat[13] * mat[3] * mat[6];

	invOut[3] = -mat[1] * mat[6] * mat[11] +
		mat[1] * mat[7] * mat[10] +
		mat[5] * mat[2] * mat[11] -
		mat[5] * mat[3] * mat[10] -
		mat[9] * mat[2] * mat[7] +
		mat[9] * mat[3] * mat[6];

	invOut[4] = -mat[4] * mat[10] * mat[15] +
		mat[4] * mat[11] * mat[14] +
		mat[8] * mat[6] * mat[15] -
		mat[8] * mat[7] * mat[14] -
		mat[12] * mat[6] * mat[11] +
		mat[12] * mat[7] * mat[10];

	invOut[5] = mat[0] * mat[10] * mat[15] -
		mat[0] * mat[11] * mat[14] -
		mat[8] * mat[2] * mat[15] +
		mat[8] * mat[3] * mat[14] +
		mat[12] * mat[2] * mat[11] -
		mat[12] * mat[3] * mat[10];

	invOut[6] = -mat[0] * mat[6] * mat[15] +
		mat[0] * mat[7] * mat[14] +
		mat[4] * mat[2] * mat[15] -
		mat[4] * mat[3] * mat[14] -
		mat[12] * mat[2] * mat[7] +
		mat[12] * mat[3] * mat[6];

	invOut[7] = mat[0] * mat[6] * mat[11] -
		mat[0] * mat[7] * mat[10] -
		mat[4] * mat[2] * mat[11] +
		mat[4] * mat[3] * mat[10] +
		mat[8] * mat[2] * mat[7] -
		mat[8] * mat[3] * mat[6];

	invOut[8] = mat[4] * mat[9] * mat[15] -
		mat[4] * mat[11] * mat[13] -
		mat[8] * mat[5] * mat[15] +
		mat[8] * mat[7] * mat[13] +
		mat[12] * mat[5] * mat[11] -
		mat[12] * mat[7] * mat[9];

	invOut[9] = -mat[0] * mat[9] * mat[15] +
		mat[0] * mat[11] * mat[13] +
		mat[8] * mat[1] * mat[15] -
		mat[8] * mat[3] * mat[13] -
		mat[12] * mat[1] * mat[11] +
		mat[12] * mat[3] * mat[9];

	invOut[10] = mat[0] * mat[5] * mat[15] -
		mat[0] * mat[7] * mat[13] -
		mat[4] * mat[1] * mat[15] +
		mat[4] * mat[3] * mat[13] +
		mat[12] * mat[1] * mat[7] -
		mat[12] * mat[3] * mat[5];

	invOut[11] = -mat[0] * mat[5] * mat[11] +
		mat[0] * mat[7] * mat[9] +
		mat[4] * mat[1] * mat[11] -
		mat[4] * mat[3] * mat[9] -
		mat[8] * mat[1] * mat[7] +
		mat[8] * mat[3] * mat[5];

	invOut[12] = -mat[4] * mat[9] * mat[14] +
		mat[4] * mat[10] * mat[13] +
		mat[8] * mat[5] * mat[14] -
		mat[8] * mat[6] * mat[13] -
		mat[12] * mat[5] * mat[10] +
		mat[12] * mat[6] * mat[9];

	invOut[13] = mat[0] * mat[9] * mat[14] -
		mat[0] * mat[10] * mat[13] -
		mat[8] * mat[1] * mat[14] +
		mat[8] * mat[2] * mat[13] +
		mat[12] * mat[1] * mat[10] -
		mat[12] * mat[2] * mat[9];

	invOut[14] = -mat[0] * mat[5] * mat[14] +
		mat[0] * mat[6] * mat[13] +
		mat[4] * mat[1] * mat[14] -
		mat[4] * mat[2] * mat[13] -
		mat[12] * mat[1] * mat[6] +
		mat[12] * mat[2] * mat[5];

	invOut[15] = mat[0] * mat[5] * mat[10] -
		mat[0] * mat[6] * mat[9] -
		mat[4] * mat[1] * mat[10] +
		mat[4] * mat[2] * mat[9] +
		mat[8] * mat[1] * mat[6] -
		mat[8] * mat[2] * mat[5];

	float det = mat[0] * invOut[0] + mat[1] * invOut[4] + mat[2] * invOut[8] + mat[3] * invOut[12];
	if (det == 0.0f)
	{
		// 逆行列なし（特異行列）
		return MakeIdentity4x4(); // または assert, エラーログ等
	}

	float invDet = 1.0f / det;
	for (int i = 0; i < 16; ++i)
	{
		inv[i] = invOut[i] * invDet;
	}

	return result;
}
DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);

	// ファイルパスの確認ログ
	Log(std::format(L"Attempting to load texture from: {}", filePathW));

	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);

	if (FAILED(hr)) {
		Log(std::format(L"Failed to load texture. HRESULT: {}", hr));
		// 詳細なエラーメッセージを追加
		return image;  // エラー処理。適切な返り値を返す
	}

	// 成功時の処理
	assert(SUCCEEDED(hr));


	// ミップマップの作成
	DirectX::ScratchImage mipImage{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImage);
	assert(SUCCEEDED(hr));
	return mipImage;
}
ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{
	// metadaraをもとにResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height);// Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);// 奥行きor配列Textureの配列数
	resourceDesc.Format = metadata.format;// TextureのFormat
	resourceDesc.SampleDesc.Count = 1;// サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);// Textureの次元数。普段使っているのは２次元

	// 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

	// リソース作成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	// エラー処理
	if (FAILED(hr)) {
		Log(std::format(L"Failed to create committed resource. HRESULT: {}", hr));
		return nullptr;  // エラーの場合、nullptrを返す
	}

	// リソースが正常に作成されたか確認
	assert(resource != nullptr);
	return resource;
}
void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{
	// Meta情報を取得
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	// 全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel)
	{
		// MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		// Textureに転送
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),
			nullptr,
			img->pixels,
			UINT(img->rowPitch),
			UINT(img->slicePitch)
		);
		assert(SUCCEEDED(hr));
	}
}
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearZ, float farZ) {
	Matrix4x4 result{};

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farZ - nearZ);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearZ / (nearZ - farZ);
	result.m[3][3] = 1.0f;

	return result;
}
