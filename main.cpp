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
#include <d3d12.h>
#include <d3d12shader.h>
#include <wrl.h>

#include "Math.h"
#include "Types.h"
#include "ModelLoader.h"
#include "SoundManager.h"
#include "TextureManager.h"
#include "Util.h"
#include "ShaderCompiler.h"

using Microsoft::WRL::ComPtr;
// 必要なライブラリリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib, "xinput.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern std::vector<std::vector<D3D12_VERTEX_BUFFER_VIEW>> vertexBufferViewsPerModel;

const char* textureNames[] = {
	"uvChecker",
	"monsterBall",
	"checkerBoard"
};

extern std::vector<ModelData> allModels;

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

ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

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

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

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
SoundManager soundManager;
SoundData soundData1;

DisplayMode currentMode = DisplayMode::Sprite;
LightingType currentLighting = LightingType::Lambert;



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


	char exePath[MAX_PATH]{};
	GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
	std::filesystem::current_path(exeDir);


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
	ImGui_ImplDX12_Init(device, swapChainDesc.BufferCount, DXGI_FORMAT_R8G8B8A8_UNORM, srvDescriptorHeap, srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	
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
	DirectX::ScratchImage mipImages = TextureManager::LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource = TextureManager::CreateTextureResource(device, metadata);
	TextureManager::UploadTextureData(textureResource, mipImages);

	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = TextureManager::LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ID3D12Resource* textureResource2 = TextureManager::CreateTextureResource(device, metadata2);
	TextureManager::UploadTextureData(textureResource2, mipImages2);

	// checkerBoard.png を読み込む
	DirectX::ScratchImage mipImages3 = TextureManager::LoadTexture("Resources/checkerBoard.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	ID3D12Resource* textureResource3 = TextureManager::CreateTextureResource(device, metadata3);
	TextureManager::UploadTextureData(textureResource3, mipImages3);

	if (!soundManager.Initialize()) {
		MessageBoxA(nullptr, "XAudio2 Init Failed", "Error", MB_OK);
		return -1;
	}

	soundData1 = soundManager.SoundLoadWave("Resources/fanfare.wav");

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

	// checkerBoard.png のメタデータを元にSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata3.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	// SRVを作成するDescriptorHeap内の3番目のスロットを指定
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);

	// SRV作成
	device->CreateShaderResourceView(textureResource3, &srvDesc3, textureSrvHandleCPU3);


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


	// --- Sprite用のリソースとビューを作成 ---
	ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);
	ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

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
	ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialDataSprite->lightingType = 0;
	

	// Sprite用のTransformationMatrix用のリソースを作る
	ID3D12Resource* transformationMatrixResourceSprite =
		CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

	// 通常モデル用のマテリアルリソースを作成
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
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
	ID3D12Resource* wvpResourceSphere = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataSphere = nullptr;
	wvpResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataSphere));
	wvpDataSphere->WVP = MakeIdentity4x4();
	wvpDataSphere->World = MakeIdentity4x4();

	// モデル用（Model）
	ID3D12Resource* wvpResourceModel = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataModel = nullptr;
	wvpResourceModel->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataModel));
	wvpDataModel->WVP = MakeIdentity4x4();
	wvpDataModel->World = MakeIdentity4x4();

	ID3D12Resource* wvpResourceTeapot = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataTeapot = nullptr;
	wvpResourceTeapot->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataTeapot));
	wvpDataTeapot->WVP = MakeIdentity4x4();
	wvpDataTeapot->World = MakeIdentity4x4();

	ID3D12Resource* wvpResourceBunny = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataBunny = nullptr;
	wvpResourceBunny->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataBunny));
	wvpDataBunny->WVP = MakeIdentity4x4();
	wvpDataBunny->World = MakeIdentity4x4();

	ID3D12Resource* wvpResourceMultiMesh = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataMultiMesh = nullptr;
	wvpResourceMultiMesh->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataMultiMesh));
	wvpDataMultiMesh->WVP = MakeIdentity4x4();
	wvpDataMultiMesh->World = MakeIdentity4x4();


	// ライト用の定数バッファリソースを作成
	ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
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

	// ブレンドステイト（アルファブレンド有効）
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;



	// ラスタライザーステイト
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = ShaderCompiler::CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);
	IDxcBlob* pixelShaderBlob = ShaderCompiler::CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
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


	// 不透明用ブレンド
	D3D12_BLEND_DESC blendOpaque{};
	blendOpaque.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendOpaque.RenderTarget[0].BlendEnable = FALSE;  // 不透明はブレンドOFF

	// 深度（不透明は書き込む）
	D3D12_DEPTH_STENCIL_DESC dsOpaque{};
	dsOpaque.DepthEnable = TRUE;
	dsOpaque.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	dsOpaque.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// ================================
	// 3D 半透明 用 PSO（深度は読むだけ）
	// ================================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoAlpha3D = {};
	psoAlpha3D.pRootSignature = rootSignature;
	psoAlpha3D.InputLayout = inputLayoutDesc;

	// Shaders
	psoAlpha3D.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoAlpha3D.PS = { pixelShaderBlob->GetBufferPointer(),  pixelShaderBlob->GetBufferSize() };

	// Rasterizer
	psoAlpha3D.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoAlpha3D.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoAlpha3D.RasterizerState.FrontCounterClockwise = FALSE;
	psoAlpha3D.RasterizerState.DepthBias = 0;
	psoAlpha3D.RasterizerState.DepthBiasClamp = 0.0f;
	psoAlpha3D.RasterizerState.SlopeScaledDepthBias = 0.0f;
	psoAlpha3D.RasterizerState.DepthClipEnable = TRUE;
	psoAlpha3D.RasterizerState.MultisampleEnable = FALSE;
	psoAlpha3D.RasterizerState.AntialiasedLineEnable = FALSE;
	psoAlpha3D.RasterizerState.ForcedSampleCount = 0;
	psoAlpha3D.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// Blend（ストレートα）
	psoAlpha3D.BlendState.AlphaToCoverageEnable = FALSE;
	psoAlpha3D.BlendState.IndependentBlendEnable = FALSE;
	psoAlpha3D.BlendState.RenderTarget[0].BlendEnable = TRUE;
	psoAlpha3D.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoAlpha3D.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoAlpha3D.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoAlpha3D.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoAlpha3D.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoAlpha3D.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoAlpha3D.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	// DepthStencil（読むだけ）
	psoAlpha3D.DepthStencilState.DepthEnable = TRUE;
	psoAlpha3D.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 読むだけ
	psoAlpha3D.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoAlpha3D.DepthStencilState.StencilEnable = FALSE;
	psoAlpha3D.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoAlpha3D.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	// RTV/DSV
	psoAlpha3D.NumRenderTargets = 1;
	psoAlpha3D.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoAlpha3D.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Topology / Sample
	psoAlpha3D.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoAlpha3D.SampleDesc.Count = 1;
	psoAlpha3D.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ID3D12PipelineState* psoAlpha3DPso = nullptr;
	hr = device->CreateGraphicsPipelineState(&psoAlpha3D, IID_PPV_ARGS(&psoAlpha3DPso));
	assert(SUCCEEDED(hr));


	// =====================================
	// HUD / スプライト 用 PSO（深度テストOFF）
	// =====================================
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoAlphaHUD = psoAlpha3D; // ベースコピー

	// そのままだと DSVFormat を参照してしまうので、深度を切る設定に明示的に上書き
	psoAlphaHUD.DepthStencilState.DepthEnable = FALSE;
	psoAlphaHUD.DepthStencilState.StencilEnable = FALSE;
	// HUDは深度使わないので DSVFormat をそのままでも動くが、気持ち悪ければゼロ化でもOK
	// psoAlphaHUD.DSVFormat = DXGI_FORMAT_UNKNOWN; // ←DSV未使用を明示したい場合

	ID3D12PipelineState* psoAlphaSprite = nullptr;
	hr = device->CreateGraphicsPipelineState(&psoAlphaHUD, IID_PPV_ARGS(&psoAlphaSprite));
	assert(SUCCEEDED(hr));


	// PSOベース
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(),  pixelShaderBlob->GetBufferSize() };
	psoDesc.BlendState = blendOpaque;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = dsOpaque;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // あなたの設定に合わせる
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ID3D12PipelineState* psoOpaque = nullptr;
	hr=(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&psoOpaque)));

	// アルファ用 PSO の組み立て
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoAlphaDesc = {};
	psoAlphaDesc.pRootSignature = rootSignature;
	psoAlphaDesc.InputLayout = inputLayoutDesc;
	psoAlphaDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoAlphaDesc.PS = { pixelShaderBlob->GetBufferPointer(),  pixelShaderBlob->GetBufferSize() };
	psoAlphaDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoAlphaDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	// ★ブレンド（OK）
	psoAlphaDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoAlphaDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	psoAlphaDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoAlphaDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoAlphaDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoAlphaDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoAlphaDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoAlphaDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	// ★深度（読むだけ）
	psoAlphaDesc.DepthStencilState.DepthEnable = TRUE;
	psoAlphaDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoAlphaDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	psoAlphaDesc.NumRenderTargets = 1;
	psoAlphaDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoAlphaDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoAlphaDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoAlphaDesc.SampleDesc.Count = 1;

	// ★これが無いと“出ません”
	psoAlphaDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;  // = 0xFFFFFFFF


	ID3D12PipelineState* psoAlpha = nullptr;
	device->CreateGraphicsPipelineState(&psoAlphaDesc, IID_PPV_ARGS(&psoAlpha));



	// 実際に生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	ID3D12Resource* vertexResourceSphere = CreateBufferResource(
		device, sizeof(VertexData) * vertexDataSphere.size());

	VertexData* vertexDataSpherePtr = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSpherePtr));
	memcpy(vertexDataSpherePtr, vertexDataSphere.data(), sizeof(VertexData) * vertexDataSphere.size());
	vertexResourceSphere->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(vertexDataSphere.size());
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	ModelData modelData = ModelLoader::LoadObjFile("resources", "plane.obj");
	ModelData teapotModel = ModelLoader::LoadObjFile("resources", "teapot.obj");
	ModelData modelDataBunny = ModelLoader::LoadObjFile("Resources", "bunny.obj");
	ModelData multiMeshModel = ModelLoader::LoadObjFile("Resources", "multiMesh.obj");



	// モデル一覧
	std::vector<ModelData> allModels = {
		modelData,         // 旧: modelData
		teapotModel,       // 旧: teapotModel
		modelDataBunny,    // 旧: modelDataBunny
		multiMeshModel     // 旧: multiMeshModel
	};

	// 結果保存用
	std::vector<std::vector<ID3D12Resource*>> vertexResourcesPerModel;
	std::vector<std::vector<D3D12_VERTEX_BUFFER_VIEW>> vertexBufferViewsPerModel;

	for (const auto& model : allModels) {
		std::vector<ID3D12Resource*> vertexResources;
		std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews;

		for (const auto& mesh : model.meshes) {
			// 頂点バッファリソース作成
			ID3D12Resource* vertexResource = CreateBufferResource(
				device, sizeof(VertexData) * mesh.vertices.size());
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

	bool useMonsterBall = false;

	// --- メインループ ---
	MSG msg{};
	bool wasYPressed = false;
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator, graphicsPipelineState);
			assert(SUCCEEDED(hr));

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
					soundManager.SoundPlayWave(soundData1);
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

			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);  // カメラをZ方向に引く
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
				0.45f,
				float(kClientWidth) / float(kClientHeight),
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
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

			transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;


			commandList->SetGraphicsRootSignature(rootSignature);
			// マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

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

			D3D12_GPU_DESCRIPTOR_HANDLE textureSRVs[] = {
	             textureSrvHandleGPU,   // uvChecker
	             textureSrvHandleGPU2,  // monsterBall
	             textureSrvHandleGPU3   // checkerBoard
			};

			const char* textureNames[] = { "uvChecker", "monsterBall", "checkerBoard" };
			static int selectedTextureIndex = 0;

			// --- DrawCall 共通設定 ---
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);

			// SRVヒープ
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// デフォルトのSRV（必要に応じて各描画直前に上書き）
			commandList->SetGraphicsRootDescriptorTable(3, textureSRVs[selectedTextureIndex]);

			// マテリアル(b0), ライト(b1) は共通
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());

			// ==========================================================
			// 1) 不透明パス（DepthEnable=TRUE, DepthWrite=ALL, Blend=OFF）
			// ==========================================================
			// A が十分小さいなら半透明扱い
			const bool isPlaneTransparent = (materialData->color.w < 0.999f);


			switch (currentMode)
			{
			case DisplayMode::Sprite:
			{
				// 不透明パス
				if (!isPlaneTransparent) {
					commandList->SetPipelineState(psoOpaque);                  // 深度書き込みON, Blend OFF
					commandList->SetGraphicsRootConstantBufferView(2, wvpResourceModel->GetGPUVirtualAddress());
					commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[0][0]);
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->SetGraphicsRootDescriptorTable(3, textureSRVs[selectedTextureIndex]);
					commandList->DrawInstanced((UINT)allModels[0].meshes[0].vertices.size(), 1, 0, 0);
				}

				// 半透明パス（必要なときだけ）
				if (isPlaneTransparent) {
					commandList->SetPipelineState(psoAlpha3DPso);              // 深度書き込みOFF, Blend ON
					commandList->SetGraphicsRootConstantBufferView(2, wvpResourceModel->GetGPUVirtualAddress());
					commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[0][0]);
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->SetGraphicsRootDescriptorTable(3, textureSRVs[selectedTextureIndex]);
					commandList->DrawInstanced((UINT)allModels[0].meshes[0].vertices.size(), 1, 0, 0);
				}

			}

			break;

			case DisplayMode::Sphere:
			{
				// Sphere（不透明）
				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceSphere->GetGPUVirtualAddress());
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->DrawInstanced(static_cast<UINT>(vertexDataSphere.size()), 1, 0, 0);

				// Plane（不透明・床など）
				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceModel->GetGPUVirtualAddress());
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[0][0]);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->DrawInstanced(static_cast<UINT>(allModels[0].meshes[0].vertices.size()), 1, 0, 0);
			}
			break;

			case DisplayMode::Teapot:
			{
				// Teapot（不透明）
				Matrix4x4 worldMatrixTeapot = MakeAffineMatrix(teapotTransform.scale, teapotTransform.rotate, teapotTransform.translate);
				wvpDataTeapot->WVP = Multiply(worldMatrixTeapot, Multiply(viewMatrix, projectionMatrix));
				wvpDataTeapot->World = worldMatrixTeapot;

				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceTeapot->GetGPUVirtualAddress());
				int modelIndex = 1;
				for (size_t i = 0; i < vertexBufferViewsPerModel[modelIndex].size(); ++i) {
					commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[modelIndex][i]);
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->DrawInstanced(static_cast<UINT>(allModels[modelIndex].meshes[i].vertices.size()), 1, 0, 0);
				}
			}
			break;

			case DisplayMode::Bunny:
			{
				// Bunny（不透明）
				Matrix4x4 worldMatrixBunny = MakeAffineMatrix(bunnyTransform.scale, bunnyTransform.rotate, bunnyTransform.translate);
				wvpDataBunny->WVP = Multiply(worldMatrixBunny, Multiply(viewMatrix, projectionMatrix));
				wvpDataBunny->World = worldMatrixBunny;

				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceBunny->GetGPUVirtualAddress());
				int modelIndex = 2;
				for (size_t i = 0; i < vertexBufferViewsPerModel[modelIndex].size(); ++i) {
					commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[modelIndex][i]);
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->DrawInstanced(static_cast<UINT>(allModels[modelIndex].meshes[i].vertices.size()), 1, 0, 0);
				}
			}
			break;

			case DisplayMode::MultiMesh:
			{
				// MultiMesh（不透明）
				Matrix4x4 worldMatrixMultiMesh = MakeAffineMatrix(multiMeshTransform.scale, multiMeshTransform.rotate, multiMeshTransform.translate);
				wvpDataMultiMesh->WVP = Multiply(worldMatrixMultiMesh, Multiply(viewMatrix, projectionMatrix));
				wvpDataMultiMesh->World = worldMatrixMultiMesh;

				commandList->SetGraphicsRootConstantBufferView(2, wvpResourceMultiMesh->GetGPUVirtualAddress());
				int modelIndex = 3;
				for (size_t i = 0; i < vertexBufferViewsPerModel[modelIndex].size(); ++i) {
					commandList->IASetVertexBuffers(0, 1, &vertexBufferViewsPerModel[modelIndex][i]);
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->DrawInstanced(static_cast<UINT>(allModels[modelIndex].meshes[i].vertices.size()), 1, 0, 0);
				}
			}
			break;
			default:
				break;
			}

			// ==========================================================
			// 3) HUD / スプライト パス（DepthEnable=FALSE, Blend=ON）
			// ==========================================================
			if (currentMode == DisplayMode::Sprite)
			{
				commandList->SetPipelineState(psoAlphaSprite);

				// スプライトの VB/IB
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
				commandList->IASetIndexBuffer(&indexBufferViewSprite);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				// スプライト用マテリアル(b0), 行列(b2)
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(2, transformationMatrixResourceSprite->GetGPUVirtualAddress());

				// スプライトのテクスチャ
				commandList->SetGraphicsRootDescriptorTable(3, textureSRVs[selectedTextureIndex]);

				// 描画
				commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
			}


			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// 自作ウィンドウだけ表示する
			ImGui::Begin("Sprite Transform");

			const char* modeItems[] = { "Sprite", "Sphere", "Teapot", "Bunny","MultiMesh"};
			int currentModeIndex = static_cast<int>(currentMode);
			if (ImGui::Combo("Display Mode", &currentModeIndex, modeItems, IM_ARRAYSIZE(modeItems))) {
				currentMode = static_cast<DisplayMode>(currentModeIndex);
			}

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


			} else if (currentMode == DisplayMode::Sphere) {
				// Sphere用Object編集
				if (ImGui::CollapsingHeader("Sphere", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::SliderFloat3("##SphereTranslate", &sphereTransform.translate.x, -10.0f, 10.0f); ImGui::SameLine(); ImGui::Text("Translate");
					ImGui::SliderFloat3("##SphereRotate", &sphereTransform.rotate.x, -3.14f, 3.14f);       ImGui::SameLine(); ImGui::Text("Rotate");
					ImGui::SliderFloat3("##SphereScale", &sphereTransform.scale.x, 0.0f, 5.0f);            ImGui::SameLine(); ImGui::Text("Scale");
				}

				// Planeモデル共通で表示
				if (ImGui::CollapsingHeader("Plane", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::SliderFloat3("##PlaneTranslate", &modelTransform.translate.x, -10.0f, 10.0f); ImGui::SameLine(); ImGui::Text("Translate");
					ImGui::SliderFloat3("##PlaneRotate", &modelTransform.rotate.x, -3.14f, 3.14f);       ImGui::SameLine(); ImGui::Text("Rotate");
					ImGui::SliderFloat3("##PlaneScale", &modelTransform.scale.x, 0.0f, 5.0f);            ImGui::SameLine(); ImGui::Text("Scale");
				}
			}else if (currentMode == DisplayMode::Teapot) {
				ImGui::Text("Teapot Controls");
				ImGui::SliderFloat3("Teapot Translate", &teapotTransform.translate.x, -10.0f, 10.0f);
				ImGui::SliderFloat3("Teapot Rotate", &teapotTransform.rotate.x, -3.14f, 3.14f);
				ImGui::SliderFloat3("Teapot Scale", &teapotTransform.scale.x, 0.0f, 5.0f);

			} else if (currentMode == DisplayMode::Bunny){
				ImGui::Text("Bunny Controls");

				ImGui::SliderFloat3("Bunny Translate", &bunnyTransform.translate.x, -10.0f, 10.0f);
				ImGui::SliderFloat3("Bunny Rotate", &bunnyTransform.rotate.x, -3.14f, 3.14f);
				ImGui::SliderFloat3("Bunny Scale", &bunnyTransform.scale.x, 0.0f, 5.0f);
			} else if (currentMode == DisplayMode::MultiMesh) {
				ImGui::Text("MultiMesh Controls");
				ImGui::DragFloat3("MultiMesh Translate", &multiMeshTransform.translate.x, 0.01f);
				ImGui::DragFloat3("MultiMesh Rotate", &multiMeshTransform.rotate.x, 0.01f);
				ImGui::DragFloat3("MultiMesh Scale", &multiMeshTransform.scale.x, 0.01f);
			}

			ImGui::Combo("Texture", &selectedTextureIndex, textureNames, IM_ARRAYSIZE(textureNames));

			if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::ColorEdit3("Light Color", reinterpret_cast<float*>(&directionalLightData->color));
				ImGui::SliderFloat3("Light Dir", reinterpret_cast<float*>(&directionalLightData->direction), -1.0f, 1.0f);
				ImGui::SliderFloat("Intensity", &directionalLightData->intensity, 0.0f, 5.0f);
				

				// ライトの方向を正規化する（ImGuiで編集後に毎回）
				directionalLightData->direction = Normalize(directionalLightData->direction);

			}

			// 一括で色(A込み)をいじるなら ColorEdit4 が楽
			ImGui::ColorEdit4("Model Color", &materialData->color.x,
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
			ImGui::ColorEdit4("Sprite Color", &materialDataSprite->color.x,
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);

		
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
				soundManager.SoundPlayWave( soundData1);
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

	if (vertexResourceSphere) vertexResourceSphere->Release();
	if (graphicsPipelineState) graphicsPipelineState->Release();
	if (rootSignature) rootSignature->Release();
	if (vertexShaderBlob) vertexShaderBlob->Release();
	if (pixelShaderBlob) pixelShaderBlob->Release();
	if (signatureBlob) signatureBlob->Release();
	if (errorBlob) errorBlob->Release();
	materialResource->Release();

	for (auto& resourceList : vertexResourcesPerModel) {
		for (ID3D12Resource* res : resourceList) {
			if (res) {
				res->Release();
				res = nullptr;
			}
		}
	}


	soundManager.SoundUnload(&soundData1);
	soundManager.Finalize();

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
	if (FAILED(hr)) {
		MessageBoxA(nullptr, "CreateCommittedResource failed", "ERROR", MB_OK);
		return nullptr;
	}
	// 作成に失敗した場合はエラーログを出力
	if (FAILED(hr)) {
		Log(L"Failed to create material resource");
	} else {
		Log(L"Material resource created successfully");
	}

	assert(SUCCEEDED(hr));
	return resource;
}


