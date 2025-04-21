// 必要なヘッダー
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <cstdint>
#include <cassert>
#include <initguid.h> // GUID用
#include <dxcapi.h>
#include <string>
#include <format>

// 必要なライブラリリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")

IDxcBlob* CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring& filePath,
    // Compilerに使用するProfile
    const wchar_t* profile,
    // 初期化して生成したものを3つ
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler);

// 1. ConvertString関数
std::wstring ConvertString(const std::string& str) {
    int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], len);
    return wstr;
}

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
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

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
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    assert(SUCCEEDED(hr));

    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    assert(SUCCEEDED(hr));

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

	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

    // RTV用ディスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    assert(SUCCEEDED(hr));

    // バックバッファ取得＆RTV作成
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < 2; ++i) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
        assert(SUCCEEDED(hr));
        rtvHandles[i] = { rtvStartHandle.ptr + static_cast<SIZE_T>(i) * descriptorSize };
        device->CreateRenderTargetView(swapChainResources[i], nullptr, rtvHandles[i]);
    }

    // --- メインループ ---
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            hr = commandAllocator->Reset();
            assert(SUCCEEDED(hr));
            hr = commandList->Reset(commandAllocator, nullptr);
            assert(SUCCEEDED(hr));

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

            // 描画ターゲットセット＆クリア
            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);
            float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

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

    // リソース全開放後、LiveObjectsレポート
    IDXGIDebug1* debug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->Release();
    }

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
    IDxcIncludeHandler* includeHandler )
{
	Log(ConvertString(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile)));
}