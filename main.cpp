#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

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

// ウィンドウプロシージャ
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

    RECT wrc = { 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

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

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    assert(SUCCEEDED(hr));

    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    assert(SUCCEEDED(hr));

    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
    assert(SUCCEEDED(hr));

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

    hr = commandList->Close();
    assert(SUCCEEDED(hr));
    swapChain->Present(0, 0);

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO
		};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);

        infoQueue->Release();
    }
#endif

    // フェンス作成
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent != nullptr);

    // ディスクリプタヒープとバックバッファ設定
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    assert(SUCCEEDED(hr));

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
            // --- コマンドリスト準備 ---
            hr = commandAllocator->Reset();
            assert(SUCCEEDED(hr));
            hr = commandList->Reset(commandAllocator, nullptr);
            assert(SUCCEEDED(hr));

            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

            float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

            hr = commandList->Close();
            assert(SUCCEEDED(hr));

            // コマンドリスト実行
            ID3D12CommandList* cmdLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, cmdLists);

            // Present
            swapChain->Present(1, 0);

            // フェンスシグナル
            fenceValue++;
            hr = commandQueue->Signal(fence, fenceValue);
            assert(SUCCEEDED(hr));

            // フェンス待機
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

    return 0;
}
