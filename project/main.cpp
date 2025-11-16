// 必要なヘッダー
#include <Windows.h>
#include <filesystem>
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
#include <fstream>   // ifstream 用
#include <sstream>   // istringstream 用（後で使う）
#include <xaudio2.h>
#include <wrl.h>
#include <Xinput.h>
using Microsoft::WRL::ComPtr;
// 必要なライブラリリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib, "xinput.lib")

// クラス
#include "Math.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "D3DResourceLeakChecker.h"

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

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	float pad;
};

struct MeshData {
	std::string name;
	std::vector<VertexData> vertices;
};

struct ModelData {
	std::vector<MeshData> meshes;
};

extern std::vector<ModelData> allModels;

struct Material {
	Vector4 color;
	int32_t lightingType;     // ← ここをリネーム
	float padding[3];         // ← 既にパディング済みなのでそのままOK
	Matrix4x4 uvTransform;
};


struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

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

static Transform transformSprite = {
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f}
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

//// ウィンドウプロシージャ（標準）
//LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
//	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
//		return true; // ImGuiが処理した場合はここで返す
//	}
//
//	switch (msg) {
//	case WM_DESTROY:
//		PostQuitMessage(0);
//		return 0;
//	}
//	return DefWindowProc(hwnd, msg, wparam, lparam);
//}

//// エントリーポイント
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
//
//	// WindowsAPI初期化
//	winApp = new WinApp();
//	winApp->Initialize();
//
//	// DirectX初期化（★引数に winApp を渡す）
//	dxCommon = new DirectXCommon();
//	dxCommon->Initialize(winApp);
//
//	// メッセージループ（毎フレーム処理はまだ何もしない版）
//	MSG msg{};
//	while (msg.message != WM_QUIT) {
//		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
//			TranslateMessage(&msg);
//			DispatchMessage(&msg);
//		} else {
//			dxCommon->PreDraw();
//			// ここでモデルやスプライトの描画
//			dxCommon->PostDraw();
//
//		}
//	}
//
//	// 後始末（とりあえず最低限）
//	delete dxCommon;
//	winApp->Finalize();
//	delete winApp;
//
//	return 0;
//}

// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

	D3DResourceLeakChecker leakChecker;

	// WindowsAPI初期化
	winApp = new WinApp();
	winApp->Initialize();

	// DirectX初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	result = xAudio2->CreateMasteringVoice(&masteringVoice);
	assert(SUCCEEDED(result));

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = DirectXCommon::LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	auto textureResource = dxCommon->CreateTextureResource(metadata);
	dxCommon->UploadTextureData(textureResource, mipImages);

	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = DirectXCommon::LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	auto textureResource2 = dxCommon->CreateTextureResource(metadata2);
	dxCommon->UploadTextureData(textureResource2, mipImages2);

	// checkerBoard.png を読み込む
	DirectX::ScratchImage mipImages3 = DirectXCommon::LoadTexture("Resources/checkerBoard.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	auto textureResource3 = dxCommon->CreateTextureResource(metadata3);
	dxCommon->UploadTextureData(textureResource3, mipImages3);

	//metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// SRV用のヒープでディスクリプタの数は128.
	// DirectXCommon が作った SRV ヒープを借りる
	ID3D12DescriptorHeap* srvDescriptorHeap = dxCommon->GetSrvDescriptorHeap();

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// ★ここを追加
	ID3D12Device* device = dxCommon->GetDevice();

	// 先頭はImGuiが使っているのでその次を使う
	UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleCPU.ptr += increment;
	textureSrvHandleGPU.ptr += increment;

	// SRVを作成
	device->CreateShaderResourceView(
		textureResource.Get(),
		&srvDesc,
		textureSrvHandleCPU);


	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める（1回だけでOK）
	auto    srvHeap = dxCommon->GetSrvDescriptorHeap();
	uint32_t descriptorSizeSRV = dxCommon->GetDescriptorSizeSRV();

	// ★2枚目用（スロット2）
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 =
		DirectXCommon::GetCPUDescriptorHandle(srvHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 =
		DirectXCommon::GetGPUDescriptorHandle(srvHeap, descriptorSizeSRV, 2);

	// SRVを作成
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	// checkerBoard.png のメタデータを元にSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata3.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	// ★3枚目用（スロット3）
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 =
		DirectXCommon::GetCPUDescriptorHandle(srvHeap, descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 =
		DirectXCommon::GetGPUDescriptorHandle(srvHeap, descriptorSizeSRV, 3);

	// SRV作成
	device->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);



	// ディスクリプタヒープの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.BaseShaderRegister = 0; // t0 から
	descriptorRange.NumDescriptors = 1;
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 1. RootParameter作成（CBV b0）
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

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
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
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
			Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		} else {
			Logger::Log("Unknown error in D3D12SerializeRootSignature");
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


	// --- Sprite用のリソースとビューを作成 ---
	// --- Sprite用のリソースとビューを作成 ---
// ComPtrで受けるのが安全
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite =
		dxCommon->CreateBufferResource(sizeof(VertexData) * 4);
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite =
		dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);


	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

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
	// Sprite用マテリアルリソースを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = dxCommon->CreateBufferResource(sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataSprite->lightingType = 0;


	// Sprite用のTransformationMatrix用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

	// 通常モデル用のマテリアルリソースを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->lightingType = static_cast<int>(currentLighting); // ← 修正


	materialData->uvTransform = MakeIdentity4x4();
	materialDataSprite->uvTransform = MakeIdentity4x4();

	Transform uvTransformSprite{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};



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
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA; // ソースの値はα
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD; // 加算
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // デストの値は1-α
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// ラスタライザーステイト
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	auto vertexShaderBlob = dxCommon->CompileShader(L"Object3D.VS.hlsl", L"vs_6_0");
	auto pixelShaderBlob = dxCommon->CompileShader(L"Object3D.PS.hlsl", L"ps_6_0");

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
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

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

	// ==========================
// 半透明専用 PSO（psoAlpha）
// ==========================

// ブレンド（αブレンド）

	auto& rt0 = blendDesc.RenderTarget[0];
	rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	rt0.BlendEnable = TRUE;
	rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;       // src = α
	rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;   // dst = 1-α
	rt0.BlendOp = D3D12_BLEND_OP_ADD;
	rt0.SrcBlendAlpha = D3D12_BLEND_ONE;             // αチャンネルは足し算
	rt0.DestBlendAlpha = D3D12_BLEND_ZERO;
	rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;



	// PSO 設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature;
	desc.InputLayout = inputLayoutDesc;
	desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	desc.PS = { pixelShaderBlob->GetBufferPointer(),  pixelShaderBlob->GetBufferSize() };
	desc.BlendState = blendDesc;
	desc.RasterizerState = rasterizerDesc;

	desc.NumRenderTargets = 1;
	// ★ SwapChain と一致させる（UNORMに統一推奨）
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D12_DEPTH_STENCIL_DESC dss{};
	dss.DepthEnable = TRUE;
	dss.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// ★ 半透明は書き込みOFF（重ね順を壊さない）
	dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState = dss;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ID3D12PipelineState* psoAlpha = nullptr;
	hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&psoAlpha));
	assert(SUCCEEDED(hr));


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

	// --- メインループ ---
	bool wasYPressed = false;
	while (TRUE) {
		// メッセージ処理
		if (winApp->ProcessMessage()) {
			// break
			break;
		}

		// ゲームパッドの状態取得
		XINPUT_STATE state{};
		DWORD result = XInputGetState(0, &state); // 0は1Pコントローラー

		if (result == ERROR_SUCCESS) {
			// ----- Lスティックでカメラ位置を移動 -----
			constexpr float deadZone = 7849.0f;
			constexpr float maxStick = 32767.0f;

			float lx = static_cast<float>(state.Gamepad.sThumbLX);
			float ly = static_cast<float>(state.Gamepad.sThumbLY);

			if (std::abs(lx) < deadZone) lx = 0;
			if (std::abs(ly) < deadZone) ly = 0;

			float normalizedLX = lx / maxStick;
			float normalizedLY = ly / maxStick;

			// カメラの移動速度
			const float moveSpeed = 0.1f;

			// カメラの正面・右方向ベクトルを計算
			Vector3 forward = {
				std::sin(cameraTransform.rotate.y),
				0.0f,
				std::cos(cameraTransform.rotate.y)
			};
			Vector3 right = {
				std::cos(cameraTransform.rotate.y),
				0.0f,
				-std::sin(cameraTransform.rotate.y)
			};



			// 左スティックで前後左右移動
			cameraTransform.translate = Add(
				cameraTransform.translate,
				AddVector(forward, normalizedLY * moveSpeed)
			);
			cameraTransform.translate = Add(
				cameraTransform.translate,
				AddVector(right, normalizedLX * moveSpeed)
			);

			// ----- Rスティックでカメラ回転 -----
			float rx = static_cast<float>(state.Gamepad.sThumbRX);
			float ry = static_cast<float>(state.Gamepad.sThumbRY);

			if (std::abs(rx) < deadZone) rx = 0;
			if (std::abs(ry) < deadZone) ry = 0;

			float normalizedRX = rx / maxStick;
			float normalizedRY = ry / maxStick;

			const float rotateSpeed = 0.02f;
			cameraTransform.rotate.y += normalizedRX * rotateSpeed; // 左右旋回
			cameraTransform.rotate.x -= normalizedRY * rotateSpeed; // ✅ ここをマイナスに


			bool wasAPressed = false;
			// Aボタンが押されているか？
			bool isAPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A);

			// 押しっぱなし防止：前フレーム押されてなかった → 今押された
			if (isAPressed && !wasAPressed) {
				SoundPlayWave(xAudio2.Get(), soundData1); // サウンド再生関数
			}

			// 状態を記録
			wasAPressed = isAPressed;


			// Yボタンの状態
			bool isYPressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);

			if (isYPressed && !wasYPressed) {
				// モード切り替え（enumを循環）
				int mode = static_cast<int>(currentMode);
				mode = (mode + 1) % static_cast<int>(DisplayMode::Count); // ← Count を追加しておくと安全
				currentMode = static_cast<DisplayMode>(mode);
			}

			wasYPressed = isYPressed;


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

		// Sprite用のWVP行列を作成（正射影）
		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
		Matrix4x4 viewMatrixSprite = MakeIdentity4x4(); // スプライトはビュー不要
		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
		Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

		transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
		transformationMatrixDataSprite->World = worldMatrixSprite;

		dxCommon->PreDraw();                              // ← まずこれ

		ID3D12GraphicsCommandList* commandList =
			dxCommon->GetCommandList();                   // ← このフレームのコマンドリスト

		// ルートシグネチャ / 共通CBV
		commandList->SetGraphicsRootSignature(rootSignature);
		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());

		// SRVヒープ（必要なら再セット。PreDrawでもセットしてるのでどちらでもOK）
		ID3D12DescriptorHeap* descriptorHeaps[] = {
			dxCommon->GetSrvDescriptorHeap()
		};
		commandList->SetDescriptorHeaps(1, descriptorHeaps);

		// テクスチャ SRV（配列は今まで通りでOK）
		D3D12_GPU_DESCRIPTOR_HANDLE textureSRVs[] = {
			textureSrvHandleGPU,   // uvChecker
			textureSrvHandleGPU2,  // monsterBall
			textureSrvHandleGPU3   // checkerBoard
		};
		static int selectedTextureIndex = 0;

		// 今フレームで使うテクスチャ（ImGuiの選択を反映）
		D3D12_GPU_DESCRIPTOR_HANDLE currentTextureSrv = textureSRVs[selectedTextureIndex];


		// ---------- モードごとの描画 ----------
		if (currentMode == DisplayMode::Sprite) {

			// ==========================
			// 1. OBJ（Plane）を描く → 不透明 PSO を使う
			// ==========================
			commandList->SetPipelineState(graphicsPipelineState); // ★ 不透明PSOに変更！

			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(2, wvpResourceModel->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(3, currentTextureSrv);

			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[0][0]);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced((UINT)allModels[0].meshes[0].vertices.size(), 1, 0, 0);


			// ==========================
			// 2. スプライト → 半透明 PSO
			// ==========================
			commandList->SetPipelineState(psoAlpha); // ★ スプライトは半透明

			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			commandList->IASetIndexBuffer(&indexBufferViewSprite);

			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(2, transformationMatrixResourceSprite->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(3, currentTextureSrv);

			commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
		}


		//else if (currentMode == DisplayMode::Sphere) {
		//	// --- 球（Sphere.obj）描画 ---
		//	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(2, wvpResourceSphere->GetGPUVirtualAddress());

		//	commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
		//	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//	commandList->DrawInstanced(static_cast<UINT>(vertexDataSphere.size()), 1, 0, 0);

		//	// --- モデル（Plane.obj）描画（影などのため）---
		//	commandList->SetGraphicsRootConstantBufferView(2, wvpResourceModel->GetGPUVirtualAddress());
		//	commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[0][0]); // modelData（Plane）
		//	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//	commandList->DrawInstanced(static_cast<UINT>(allModels[0].meshes[0].vertices.size()), 1, 0, 0);

		//} else if (currentMode == DisplayMode::Teapot) {
		//	// --- ティーポット描画 ---
		//	Matrix4x4 worldMatrixTeapot = MakeAffineMatrix(teapotTransform.scale, teapotTransform.rotate, teapotTransform.translate);
		//	wvpDataTeapot->WVP = Multiply(worldMatrixTeapot, Multiply(viewMatrix, projectionMatrix));
		//	wvpDataTeapot->World = worldMatrixTeapot;

		//	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(2, wvpResourceTeapot->GetGPUVirtualAddress());

		//	int modelIndex = 1; // teapotModel
		//	for (size_t i = 0; i < vertexBufferViewsPerModel[modelIndex].size(); ++i) {
		//		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[modelIndex][i]);
		//		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//		commandList->DrawInstanced(static_cast<UINT>(allModels[modelIndex].meshes[i].vertices.size()), 1, 0, 0);
		//	}

		//} else if (currentMode == DisplayMode::Bunny) {
		//	// --- バニー描画 ---
		//	Matrix4x4 worldMatrixBunny = MakeAffineMatrix(bunnyTransform.scale, bunnyTransform.rotate, bunnyTransform.translate);
		//	wvpDataBunny->WVP = Multiply(worldMatrixBunny, Multiply(viewMatrix, projectionMatrix));
		//	wvpDataBunny->World = worldMatrixBunny;

		//	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(2, wvpResourceBunny->GetGPUVirtualAddress());

		//	int modelIndex = 2; // modelDataBunny
		//	for (size_t i = 0; i < vertexBufferViewsPerModel[modelIndex].size(); ++i) {
		//		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[modelIndex][i]);
		//		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//		commandList->DrawInstanced(static_cast<UINT>(allModels[modelIndex].meshes[i].vertices.size()), 1, 0, 0);
		//	}

		//} else if (currentMode == DisplayMode::MultiMesh) {
		//	// --- マルチメッシュ描画 ---
		//	Matrix4x4 worldMatrixMultiMesh = MakeAffineMatrix(multiMeshTransform.scale, multiMeshTransform.rotate, multiMeshTransform.translate);
		//	wvpDataMultiMesh->WVP = Multiply(worldMatrixMultiMesh, Multiply(viewMatrix, projectionMatrix));
		//	wvpDataMultiMesh->World = worldMatrixMultiMesh;

		//	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(2, wvpResourceMultiMesh->GetGPUVirtualAddress());

		//	int modelIndex = 3; // multiMeshModel
		//	for (size_t i = 0; i < vertexBufferViewsPerModel[modelIndex].size(); ++i) {
		//		commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[modelIndex][i]);
		//		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//		commandList->DrawInstanced(static_cast<UINT>(allModels[modelIndex].meshes[i].vertices.size()), 1, 0, 0);
		//	}
		//}

		//描画

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 自作ウィンドウだけ表示する
		ImGui::Begin("Sprite Transform");

		/*const char* modeItems[] = { "Sprite", "Sphere", "Teapot", "Bunny","MultiMesh" };
		int currentModeIndex = static_cast<int>(currentMode);
		if (ImGui::Combo("Display Mode", &currentModeIndex, modeItems, IM_ARRAYSIZE(modeItems))) {
			currentMode = static_cast<DisplayMode>(currentModeIndex);
		}*/


		// === モード別UI分岐 ===
		if (currentMode == DisplayMode::Sprite) {
			ImGui::Text("Create");
			ImGui::Separator();

			// Object (Plane)
			if (ImGui::CollapsingHeader("Plane", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::SliderFloat3("##PlaneTranslate", &modelTransform.translate.x, -100.0f, 100.0f); ImGui::SameLine(); ImGui::Text("Translate");
				ImGui::SliderFloat3("##PlaneRotate", &modelTransform.rotate.x, -3.14f, 3.14f);         ImGui::SameLine(); ImGui::Text("Rotate");
				ImGui::SliderFloat3("##PlaneScale", &modelTransform.scale.x, 0.0f, 5.0f);              ImGui::SameLine(); ImGui::Text("Scale");
			}

			// Material
			if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::SliderFloat3("##SpriteTranslate", &transformSprite.translate.x, -100.0f, 100.0f); ImGui::SameLine(); ImGui::Text("Translate");
				ImGui::SliderFloat3("##SpriteRotate", &transformSprite.rotate.x, -3.14f, 3.14f);         ImGui::SameLine(); ImGui::Text("Rotate");
				ImGui::SliderFloat3("##SpriteScale", &transformSprite.scale.x, 0.0f, 5.0f);              ImGui::SameLine(); ImGui::Text("Scale");

			}

			// UVTranslate（2D）
			ImGui::DragFloat2("##UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::SameLine(); ImGui::Text("UVTranslate");

			// UVRotate（Z軸のみ）
			ImGui::DragFloat("##UVRotate", &uvTransformSprite.rotate.z, 0.01f, -3.14f, 3.14f);
			ImGui::SameLine(); ImGui::Text("UVRotate");

			// UVScale（2D）
			ImGui::DragFloat2("##UVScale", &uvTransformSprite.scale.x, 0.01f, 0.0f, 10.0f);
			ImGui::SameLine(); ImGui::Text("UVScale");


			//} else if (currentMode == DisplayMode::Sphere) {
			//	// Sphere用Object編集
			//	if (ImGui::CollapsingHeader("Sphere", ImGuiTreeNodeFlags_DefaultOpen)) {
			//		ImGui::SliderFloat3("##SphereTranslate", &sphereTransform.translate.x, -10.0f, 10.0f); ImGui::SameLine(); ImGui::Text("Translate");
			//		ImGui::SliderFloat3("##SphereRotate", &sphereTransform.rotate.x, -3.14f, 3.14f);       ImGui::SameLine(); ImGui::Text("Rotate");
			//		ImGui::SliderFloat3("##SphereScale", &sphereTransform.scale.x, 0.0f, 5.0f);            ImGui::SameLine(); ImGui::Text("Scale");
			//	}

			//	// Planeモデル共通で表示
			//	if (ImGui::CollapsingHeader("Plane", ImGuiTreeNodeFlags_DefaultOpen)) {
			//		ImGui::SliderFloat3("##PlaneTranslate", &modelTransform.translate.x, -10.0f, 10.0f); ImGui::SameLine(); ImGui::Text("Translate");
			//		ImGui::SliderFloat3("##PlaneRotate", &modelTransform.rotate.x, -3.14f, 3.14f);       ImGui::SameLine(); ImGui::Text("Rotate");
			//		ImGui::SliderFloat3("##PlaneScale", &modelTransform.scale.x, 0.0f, 5.0f);            ImGui::SameLine(); ImGui::Text("Scale");
			//	}
			//} else if (currentMode == DisplayMode::Teapot) {
			//	ImGui::Text("Teapot Controls");
			//	ImGui::SliderFloat3("Teapot Translate", &teapotTransform.translate.x, -10.0f, 10.0f);
			//	ImGui::SliderFloat3("Teapot Rotate", &teapotTransform.rotate.x, -3.14f, 3.14f);
			//	ImGui::SliderFloat3("Teapot Scale", &teapotTransform.scale.x, 0.0f, 5.0f);

			//} else if (currentMode == DisplayMode::Bunny) {
			//	ImGui::Text("Bunny Controls");

			//	ImGui::SliderFloat3("Bunny Translate", &bunnyTransform.translate.x, -10.0f, 10.0f);
			//	ImGui::SliderFloat3("Bunny Rotate", &bunnyTransform.rotate.x, -3.14f, 3.14f);
			//	ImGui::SliderFloat3("Bunny Scale", &bunnyTransform.scale.x, 0.0f, 5.0f);
			//} else if (currentMode == DisplayMode::MultiMesh) {
			//	ImGui::Text("MultiMesh Controls");
			//	ImGui::DragFloat3("MultiMesh Translate", &multiMeshTransform.translate.x, 0.01f);
			//	ImGui::DragFloat3("MultiMesh Rotate", &multiMeshTransform.rotate.x, 0.01f);
			//	ImGui::DragFloat3("MultiMesh Scale", &multiMeshTransform.scale.x, 0.01f);
			//}

			ImGui::Combo("Texture", &selectedTextureIndex, textureNames, IM_ARRAYSIZE(textureNames));

			if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::ColorEdit3("Light Color", reinterpret_cast<float*>(&directionalLightData->color));
				ImGui::SliderFloat3("Light Dir", reinterpret_cast<float*>(&directionalLightData->direction), -1.0f, 1.0f);
				ImGui::SliderFloat("Intensity", &directionalLightData->intensity, 0.0f, 5.0f);

				// ライトの方向を正規化する（ImGuiで編集後に毎回）
				directionalLightData->direction = Normalize(directionalLightData->direction);

			}
			static float alphaValue = 1.0f;
			ImGui::SliderFloat("Alpha", &alphaValue, 0.0f, 1.0f, "%.2f");
			materialData->color.w = alphaValue;
			materialDataSprite->color.w = alphaValue; // スプライトも同様なら


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

			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

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

	// D3D12 リソース（Rawポインタだけ解放する）
	if (psoAlpha) {
		psoAlpha->Release();
		psoAlpha = nullptr;
	}
	if (graphicsPipelineState) {
		graphicsPipelineState->Release();
		graphicsPipelineState = nullptr;
	}
	if (rootSignature) {
		rootSignature->Release();
		rootSignature = nullptr;
	}

	if (vertexShaderBlob) {
		vertexShaderBlob->Release();
		vertexShaderBlob = nullptr;
	}
	if (pixelShaderBlob) {
		pixelShaderBlob->Release();
		pixelShaderBlob = nullptr;
	}
	if (signatureBlob) {
		signatureBlob->Release();
		signatureBlob = nullptr;
	}
	if (errorBlob) {
		errorBlob->Release();
		errorBlob = nullptr;
	}

	// ※ materialResourceSprite, transformationMatrixResourceSprite,
	//    materialResource, wvpResourceXXX, directionalLightResource,
	//    vertexResourceSphere, vertexResourcesPerModel などは
	//    すべて ComPtr なので Release 不要（スコープ終了で自動解放）

	delete dxCommon;   dxCommon = nullptr;
	delete input;      input = nullptr;

	// LiveObjects の出力は D3DResourceLeakChecker に任せるのでここは削除
	// （IDXGIDebug1* をここで触らない）

	if (winApp) {
		winApp->Finalize();
		delete winApp;
		winApp = nullptr;
	}

	return 0;
}



