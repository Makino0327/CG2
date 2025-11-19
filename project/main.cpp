// 標準ライブラリ
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <fstream>
#include <sstream>

#define _USE_MATH_DEFINES
#include <cmath>      // sinf, cosf, M_PI など

// DirectX / COM 周り
#include <d3d12.h>    // ID3D12GraphicsCommandList, ID3D12DescriptorHeap などで必要
#include <xaudio2.h>
#include <Xinput.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// ImGui
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

// 自作ヘッダー
#include "Math.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "D3DResourceLeakChecker.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"

// ライブラリリンク（ここにまとめておく）
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "xinput.lib")



extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern std::vector<std::vector<D3D12_VERTEX_BUFFER_VIEW>> vertexBufferViewsPerModel;

enum class DisplayMode {
	Sprite,
	Sphere,
	Teapot,
	Bunny,
	MultiMesh,
	Count
};

const char* textureNames[] = {
	"uvChecker",
	"monsterBall",
	"checkerBoard"
};

struct ChunkHeader
{
	char id[4];
	int32_t size;
};

struct RiffHeader
{
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk
{
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

struct SoundData
{
	WAVEFORMATEX wfex;
	BYTE* pBuffer;
	unsigned int bufferSize;
};

// Lightingの方式を定義する列挙型
enum class LightingType {
	None = 0,
	Lambert,
	HalfLambert
};

struct MeshData {
	std::string name;
	std::vector<VertexData> vertices;
};

struct ModelData {
	std::vector<MeshData> meshes;
};

extern std::vector<ModelData> allModels;

struct DirectionalLight {
	Vector4 color;        // ライトの色
	Vector3 direction;    // ライトの向き（単位ベクトル）
	float intensity;      // 強度
};

// モデル（Model）のTransform
static Transform modelTransform{
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	{1.0f, 0.0f, 0.0f}
};

// 球（Sphere）のTransform
static Transform sphereTransform{
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	{-1.0f, 0.0f, 0.0f}
};

static Transform cameraTransform = {
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, -10.0f}
};

static Transform teapotTransform = {
	{1.0f, 1.0f, 1.0f},  // scale
	{0.0f, 0.0f, 0.0f},  // rotate
	{0.0f, 0.0f, 0.0f}   // translate
};

static Transform bunnyTransform = {
	{1.0f, 1.0f, 1.0f},  // scale
	{0.0f, 0.0f, 0.0f},  // rotate
	{0.0f, 0.0f, 0.0f}   // translate
};

static Transform multiMeshTransform = {
	{1.0f, 1.0f, 1.0f},  // scale
	{0.0f, 0.0f, 0.0f},  // rotate
	{0.0f, 0.0f, 0.0f}   // translate
};

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
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

Vector3 Add(const Vector3& a, const Vector3& b) {
	return {
		a.x + b.x,
		a.y + b.y,
		a.z + b.z
	};
}

Vector3 AddVector(const Vector3& v, float scalar) {
	return {
		v.x * scalar,
		v.y * scalar,
		v.z * scalar
	};
}


SoundData SoundLoadWave(const char* filename)
{
	/// 1.ファイルオープン
	// ファイル入力ストリームのインスタンス
	std::ifstream file;
	// .wavファイルをバイナリモードで開く
	file.open(filename, std::ios::binary);
	// ファイルオープン失敗を検出する
	assert(file.is_open());

	/// 2.wavデータ読み込み
	// RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
	{
		assert(0);
	}
	// タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(0);
	}
	// Formatチャンクの読み込み
	// Formatチャンクの読み込み（柔軟に探す）
	FormatChunk format = {};
	ChunkHeader chunk{};
	while (true) {
		file.read((char*)&chunk, sizeof(chunk));
		if (file.eof()) {
			assert(0 && "fmtチャンクが見つかりませんでした");
		}

		if (strncmp(chunk.id, "fmt ", 4) == 0) {
			format.chunk = chunk;
			assert(format.chunk.size <= sizeof(format.fmt));
			file.read((char*)&format.fmt, format.chunk.size);
			break;
		}

		// fmtじゃなかったらスキップ
		file.seekg(chunk.size, std::ios_base::cur);
	}

	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0)
	{
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char*)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0)
	{
		assert(0);
	}

	// Dataチャンクのデータ部（波型データ）の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);
	/// 3.ファイルクローズ
	// Waveファイルを閉じる
	file.close();

	/// 4.読み込んだ音声データをreturn
	// returnするための音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

// 音声データ解放
void SoundUnload(SoundData* soundData)
{
	// バッファの解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// 音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData)
{
	HRESULT result;

	// 波形フォーマットをもとにSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	result = pSourceVoice->Start();
}
// 1. ConvertString関数
std::wstring ConvertString(const std::string& str) {
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wstr(len, 0);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], len);
	return wstr;
}

// ★ 追加：char* から直接呼べる版
std::wstring ConvertString(const char* str) {
	return ConvertString(std::string(str));
}

// DXGI_DEBUG系のGUID定義
EXTERN_C const GUID DECLSPEC_SELECTANY DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, { 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x08 } };
EXTERN_C const GUID DECLSPEC_SELECTANY DXGI_DEBUG_APP = { 0x25cddaa4, 0xb1c6, 0x47e1, { 0xac, 0x3e, 0x98, 0xb5, 0x4d, 0x0b, 0x64, 0x2d } };
EXTERN_C const GUID DECLSPEC_SELECTANY DXGI_DEBUG_D3D12 = { 0x6d2e06cf, 0x9646, 0x4b1f, { 0xa5, 0x7e, 0xdc, 0xe2, 0x60, 0x74, 0x6c, 0xf9 } };

// グローバル変数（各種DirectXオブジェクト）
ComPtr<IXAudio2> xAudio2;
IXAudio2MasteringVoice* masteringVoice;

DisplayMode currentMode = DisplayMode::Sprite;
LightingType currentLighting = LightingType::Lambert;

SoundData soundData1 = SoundLoadWave("Resources/fanfare.wav");

HRESULT hr;

/// ポインタ
// WindowsAPI
WinApp* winApp = nullptr;
// 入力処理
Input* input = nullptr;
// DirectX共通処理
DirectXCommon* dxCommon = nullptr;
// スプライト共通処理
SpriteCommon* spriteCommon = nullptr;
// スプライト
Sprite* sprite = nullptr;
// スプライト群
std::vector<Sprite*> sprites;
// 3Dオブジェクト共通処理
Object3dCommon* object3dCommon = nullptr;
// 3Dオブジェクト
Object3d* object3d = nullptr;

// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

	D3DResourceLeakChecker leakChecker;

	// WindowsAPI初期化
	winApp = new WinApp();
	winApp->Initialize();

	// DirectX初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	// SpriteCommonの初期化
	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);

	// 3dオブジェクト共通処理の初期化
	object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);

	// 3Dオブジェクトの初期化
	object3d = new Object3d();
	object3d->Initialize();

	// テクスチャマネージャーの初期化
	TextureManager::GetInstance()->Initialize(dxCommon);

	auto texMan = TextureManager::GetInstance();
	texMan->LoadTexture("Resources/uvChecker.png");
	texMan->LoadTexture("Resources/monsterBall.png");
	texMan->LoadTexture("Resources/checkerBoard.png");


	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	result = xAudio2->CreateMasteringVoice(&masteringVoice);
	assert(SUCCEEDED(result));

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

	// 通常モデル用のマテリアルリソースを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->lightingType = static_cast<int>(currentLighting); // ← 修正


	materialData->uvTransform = MakeIdentity4x4();

	// WVP + World用の定数バッファリソースを作る
	// 球用（Sphere）
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceSphere = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataSphere = nullptr;
	wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataSphere));
	wvpDataSphere->WVP = MakeIdentity4x4();
	wvpDataSphere->World = MakeIdentity4x4();

	// モデル用（Model）
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceModel = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataModel = nullptr;
	wvpResourceModel->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataModel));
	wvpDataModel->WVP = MakeIdentity4x4();
	wvpDataModel->World = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceTeapot = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataTeapot = nullptr;
	wvpResourceTeapot->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataTeapot));
	wvpDataTeapot->WVP = MakeIdentity4x4();
	wvpDataTeapot->World = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceBunny = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataBunny = nullptr;
	wvpResourceBunny->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataBunny));
	wvpDataBunny->WVP = MakeIdentity4x4();
	wvpDataBunny->World = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResourceMultiMesh = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataMultiMesh = nullptr;
	wvpResourceMultiMesh->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataMultiMesh));
	wvpDataMultiMesh->WVP = MakeIdentity4x4();
	wvpDataMultiMesh->World = MakeIdentity4x4();


	// ライト用の定数バッファリソースを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	// 書き込み用ポインタの定義（これが必要）
	DirectionalLight* directionalLightData = nullptr;
	// マップしてアドレス取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// 初期化（単位ベクトルで）
	directionalLightData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	directionalLightData->direction = Vector3(0.0f, -1.0f, 0.0f); // 正規化されてること
	directionalLightData->intensity = 4.0f;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = dxCommon->CreateBufferResource(
		sizeof(VertexData) * vertexDataSphere.size());

	VertexData* vertexDataSpherePtr = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSpherePtr));
	memcpy(vertexDataSpherePtr, vertexDataSphere.data(), sizeof(VertexData) * vertexDataSphere.size());
	vertexResourceSphere->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(vertexDataSphere.size());
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	ModelData modelData = LoadObjFile("resources", "plane.obj");
	ModelData teapotModel = LoadObjFile("resources", "teapot.obj");
	ModelData modelDataBunny = LoadObjFile("Resources", "bunny.obj");
	ModelData multiMeshModel = LoadObjFile("Resources", "multiMesh.obj");

	// モデル一覧
	std::vector<ModelData> allModels = {
		modelData,         // 旧: modelData
		teapotModel,       // 旧: teapotModel
		modelDataBunny,    // 旧: modelDataBunny
		multiMeshModel     // 旧: multiMeshModel
	};

	// 結果保存用
	std::vector<std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>> vertexResourcesPerModel;
	std::vector<std::vector<D3D12_VERTEX_BUFFER_VIEW>> vertexBufferViewsPerModel;

	for (const auto& model : allModels) {
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> vertexResources;

		std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews;

		for (const auto& mesh : model.meshes) {
			// 頂点バッファリソース作成
			Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = dxCommon->CreateBufferResource(
				sizeof(VertexData) * mesh.vertices.size());
			vertexResources.push_back(vertexResource);

			// マッピングとコピー
			VertexData* vertexData = nullptr;
			vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
			std::memcpy(vertexData, mesh.vertices.data(),
				sizeof(VertexData) * mesh.vertices.size());
			vertexResource->Unmap(0, nullptr);

			// ビュー作成
			D3D12_VERTEX_BUFFER_VIEW vbv{};
			vbv.BufferLocation = vertexResource->GetGPUVirtualAddress();
			vbv.SizeInBytes = UINT(sizeof(VertexData) * mesh.vertices.size());
			vbv.StrideInBytes = sizeof(VertexData);
			vertexBufferViews.push_back(vbv);
		}

		vertexResourcesPerModel.push_back(vertexResources);
		vertexBufferViewsPerModel.push_back(vertexBufferViews);
	}
	bool useMonsterBall = false;

	// 入力の初期化
	input = new Input();
	input->Initialize(winApp);

	// Spriteの初期化

	for (uint32_t i = 0; i < 5; ++i) {
		Sprite* sprite = new Sprite();

		// 交互にテクスチャファイルを切り替え
		std::string texPath;
		if (i % 2 == 0) {
			texPath = "Resources/uvChecker.png";
		} else {
			texPath = "Resources/monsterBall.png";
		}

		sprite->Initialize(spriteCommon, directionalLightResource.Get(), texPath);

		Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
		sprite->SetPosition(pos);

		sprites.push_back(sprite);
	}

	// --- メインループ ---
	while (TRUE) {
		// メッセージ処理
		if (winApp->ProcessMessage()) {
			// break
			break;
		}

		input->Update();

		if (input->TriggerKey(DIK_0)) {
			OutputDebugStringA("push 0\n");
		}


		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);  // カメラをZ方向に引く
		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
			0.45f,
			float(WinApp::kClientWidth) / float(WinApp::kClientHeight),
			0.1f, 100.0f);

		Matrix4x4 worldMatrixSphere = MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
		wvpDataSphere->WVP = Multiply(worldMatrixSphere, Multiply(viewMatrix, projectionMatrix));
		wvpDataSphere->World = worldMatrixSphere;

		// モデル（Model）のWorld行列
		Matrix4x4 worldMatrixModel = MakeAffineMatrix(modelTransform.scale, modelTransform.rotate, modelTransform.translate);
		wvpDataModel->WVP = Multiply(worldMatrixModel, Multiply(viewMatrix, projectionMatrix));
		wvpDataModel->World = worldMatrixModel;

		dxCommon->PreDraw();                              // ← まずこれ

		spriteCommon->CommonDrawSetting(); // ← 共通描画設定を行う

		ID3D12GraphicsCommandList* commandList =
			dxCommon->GetCommandList();                   // ← このフレームのコマンドリスト

		// --- スプライトの基本パラメータ更新（必要なら）---
		for (Sprite* sprite : sprites) {

			Vector2 anchor = { 0.0f,0.0f };

			sprite->SetAnchorPoint(anchor);
			// 現在の座標を取得
			Vector2 position = sprite->GetPosition();
			// ※アニメさせたければここで position を変化させる
			sprite->SetPosition(position);

			// 回転
			float rotation = sprite->GetRotation();
			//rotation += 0.01f;  
			sprite->SetRotation(rotation);

			// 色
			Vector4 color = sprite->GetColor();
			// color.x += 0.01f;  など
			sprite->SetColor(color);

			// スケール
			//Vector2 size = sprite->GetSize();
			//size.x = 0.2f; // など
			//size.y = 0.3f;
			//sprite->SetSize(size);
		}

		// ---------- モードごとの描画 ----------
		if (currentMode == DisplayMode::Sprite) {

			// ==========================
			// 1. OBJ（Plane）を描く → 不透明 PSO を使う
			// ==========================
			//commandList->SetPipelineState(graphicsPipelineState); // ★ 不透明PSOに変更！

			//commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootConstantBufferView(2, wvpResourceModel->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootDescriptorTable(3, currentTextureSrv);

			//commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[0][0]);
			//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//commandList->DrawInstanced((UINT)allModels[0].meshes[0].vertices.size(), 1, 0, 0);

			object3dCommon->CommonDrawSetting(); // 3Dオブジェクト共通描画設定

			for (Sprite* sprite : sprites) {
				sprite->Update();
			}              // 位置や行列の更新
	
			for (Sprite* sprite : sprites) {
				sprite->Draw();
			}
		}


		//描画

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 自作ウィンドウだけ表示する
		ImGui::Begin("Sprite Transform");

		// === モード別UI分岐 ===
		if (currentMode == DisplayMode::Sprite) {
			ImGui::Text("Create");
			ImGui::Separator();

			// === モード別UI分岐 ===
			if (currentMode == DisplayMode::Sprite) {
				ImGui::Text("Create");
				ImGui::Separator();

				// ★ Sprite[0] だけ操作する UI ★
				if (!sprites.empty()) {

					// 0番目のスプライトを取る
					Sprite* target = sprites[0];

					// ---- Position ----
					Vector2 pos = target->GetPosition();
					if (ImGui::SliderFloat2(
						"Sprite[0] Pos",
						&pos.x,
						0.0f,
						(float)WinApp::kClientWidth))   // とりあえず画面幅を上限
					{
						target->SetPosition(pos);
					}

					// ---- Size ----
					Vector2 size = target->GetSize();
					if (ImGui::SliderFloat2(
						"Sprite[0] Size",
						&size.x,
						0.0f,
						800.0f))   // 適当に 800 くらい（必要なら変えてOK）
					{
						target->SetSize(size);
					}

					// ---- Rotation ----
					float rot = target->GetRotation();
					if (ImGui::SliderFloat(
						"Sprite[0] Rot",
						&rot,
						-3.14f, 3.14f))
					{
						target->SetRotation(rot);
					}
				}
			}


			//// Object (Plane)
			//if (ImGui::CollapsingHeader("Plane", ImGuiTreeNodeFlags_DefaultOpen)) {
			//	ImGui::SliderFloat3("##PlaneTranslate", &modelTransform.translate.x, -100.0f, 100.0f); ImGui::SameLine(); ImGui::Text("Translate");
			//	ImGui::SliderFloat3("##PlaneRotate", &modelTransform.rotate.x, -3.14f, 3.14f);         ImGui::SameLine(); ImGui::Text("Rotate");
			//	ImGui::SliderFloat3("##PlaneScale", &modelTransform.scale.x, 0.0f, 5.0f);              ImGui::SameLine(); ImGui::Text("Scale");
			//}

			//

			//if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			//	ImGui::ColorEdit3("Light Color", reinterpret_cast<float*>(&directionalLightData->color));
			//	ImGui::SliderFloat3("Light Dir", reinterpret_cast<float*>(&directionalLightData->direction), -1.0f, 1.0f);
			//	ImGui::SliderFloat("Intensity", &directionalLightData->intensity, 0.0f, 5.0f);

			//	// ライトの方向を正規化する（ImGuiで編集後に毎回）
			//	directionalLightData->direction = Normalize(directionalLightData->direction);

			//}
			//static float alphaValue = 1.0f;
			//ImGui::SliderFloat("Alpha", &alphaValue, 0.0f, 1.0f, "%.2f");
			//materialData->color.w = alphaValue;
			//materialDataSprite->color.w = alphaValue; // スプライトも同様なら


			// 現在の選択中Lighting
			static LightingType currentLighting = LightingType::HalfLambert; // 初期はLambert

			// コンボボックスの選択肢
			const char* lightingItems[] = { "None", "Lambert", "HalfLambert" };

			// 選択状態のintを取得してUIに渡す

			int currentLightingIndex = static_cast<int>(currentLighting);

			// Comboで選択
			if (ImGui::Combo("Lighting", &currentLightingIndex, lightingItems, IM_ARRAYSIZE(lightingItems))) {
				currentLighting = static_cast<LightingType>(currentLightingIndex); // 選択変更を反映
			}
			// Debug 表示
			ImGui::Separator();
			ImGui::Text("Play Sound");

			materialData->lightingType = static_cast<int>(currentLighting);

			bool isSoundPlayed = false;

			if (ImGui::Button("Start") && !isSoundPlayed) {
				SoundPlayWave(xAudio2.Get(), soundData1);
				isSoundPlayed = true;
			}

			// フレームの一番最後で呼ぶ（描画後でも可）
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
			ImGui::SetNextWindowBgAlpha(0.35f); // 半透明にする（好みで調整）

			if (ImGui::Begin("How to operate", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {

				ImGui::Text("How to operate");
				ImGui::Separator();
				ImGui::Text("Lstick : cameraTranslate");
				ImGui::Text("Rstick : cameraRotate");
				ImGui::Text("A : PlaySound");
				ImGui::Text("Y : switchOBJ");

				ImGui::End();
			}

			ImGui::End();


			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

			/*Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;*/

			dxCommon->PostDraw();

		}

	}

	// --- 後片付け --- 

// ImGuiは先に終了
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// XAudio2
	if (masteringVoice) {
		masteringVoice->DestroyVoice();
		masteringVoice = nullptr;
	}
	xAudio2.Reset();
	SoundUnload(&soundData1);

	// テクスチャマネージャーの終了
	TextureManager::GetInstance()->Finalize();

	delete spriteCommon; spriteCommon = nullptr;
	delete input;      input = nullptr;
	for (Sprite* sprite : sprites) {
		delete sprite;
	}
	sprites.clear();
	delete object3d;   object3d = nullptr;
	delete object3dCommon; object3dCommon = nullptr;

	// LiveObjects の出力は D3DResourceLeakChecker に任せるのでここは削除
	// （IDXGIDebug1* をここで触らない）
	delete dxCommon;   dxCommon = nullptr;
	if (winApp) {
		winApp->Finalize();
		delete winApp;
		winApp = nullptr;
	}

	return 0;
}



