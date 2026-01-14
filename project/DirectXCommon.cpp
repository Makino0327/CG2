#include "DirectXCommon.h"
#include <cassert>
#include <format>
#include "Logger.h" 

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"

#include "externals/DirectXTex/DirectXTex.h"
#include <thread>

const uint32_t DirectXCommon::kMaxSRVCount = 512;

using namespace Microsoft::WRL;

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


D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(
    const ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(descriptorSize) * index;
    return handle;
}

// --- SRV専用 関数 ---
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
{
    return GetCPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index)
{
    return GetGPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, index);
}

void DirectXCommon::Initialize(WinApp* winApp)
{
    assert(winApp);
    this->winApp = winApp;

	InitializeFixFPS();        // 固定FPS制御の初期化

    // --- スライドの順番通りに初期化 ---

    InitializeDevice();               // デバイスの生成
    InitializeCommand();              // コマンド関連の初期化
    InitializeSwapChain();            // スワップチェーンの生成
    InitializeDepthBuffer();          // 深度バッファの生成（※または InitializeDepthStencilView に統合）
    InitializeDescriptorHeaps();      // 各種ディスクリプタヒープの生成
    InitializeRenderTargetView();     // レンダーターゲットビューの初期化
    InitializeDepthStencilView();     // 深度ステンシルビューの初期化
    InitializeFence();                // フェンスの初期化
    InitializeScissorRect();          // シザー矩形の初期化
    InitializeDXC();                  // DXCコンパイラの生成
    //InitializeImGui();                // ImGuiの初期化
}


void DirectXCommon::InitializeDevice()
{
    HRESULT hr;

#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        // debugController->SetEnableGPUBasedValidation(TRUE); // ← 一旦コメントアウト
    }
#endif



    // DXGIファクトリ生成
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    assert(SUCCEEDED(hr));
    Logger::Log("DXGI Factory created successfully.");

    // アダプター取得（高性能GPU優先）
    ComPtr<IDXGIAdapter4> adapter;
    hr = dxgiFactory->EnumAdapterByGpuPreference(
        0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
    assert(SUCCEEDED(hr));
    Logger::Log("GPU adapter acquired.");

    // デバイス生成
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    assert(SUCCEEDED(hr));
    Logger::Log("D3D12 device created successfully.");

#ifdef _DEBUG
    // エラー時にブレーク発生を設定
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        Logger::Log("[DebugLayer] InfoQueue breakpoints enabled.");
    }
#endif
}

void DirectXCommon::InitializeCommand()
{
    HRESULT hr = S_OK;

    // コマンドキュー作成
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));
    Logger::Log("Command Queue created successfully.");

    // コマンドアロケータ作成
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));
    Logger::Log("Command Allocator created successfully.");

    // コマンドリスト作成
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
    Logger::Log("Command List created successfully.");

    // いったんクローズしておく（再利用時のため）
   // hr = commandList_->Close();
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeSwapChain()
{
    assert(winApp);
    assert(device);
    assert(commandQueue_);

    HWND hwnd = winApp->GetHwnd();
    assert(hwnd != nullptr); // ★ hwnd がちゃんと作られているか確認

    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kClientWidth;
    swapChainDesc.Height = WinApp::kClientHeight;
    // まずは昔動いていたのと同じ NON-SRGB に戻す
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // ★ここは 0 にする（ALLOW_MODE_SWITCH は使わない）
    swapChainDesc.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &tempSwapChain
    );

    if (FAILED(hr)) {
        // ここで必ず hr をログに出す
        Logger::Log(std::format(
            "CreateSwapChainForHwnd failed. hr=0x{:08X}", hr));
        assert(false); // デバッグ中は止める
    }

    hr = tempSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain_));
    assert(SUCCEEDED(hr));
}



void DirectXCommon::InitializeDepthBuffer()
{
    // DSV ヒープは InitializeDescriptorHeaps で作っている前提なら、
    // ここで新しく作らなくても OK。作っていない場合は↓でも可。
    if (!dsvDescriptorHeap_) {
        dsvDescriptorHeap_ = CreateDescriptorHeap(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            1,
            false
        );
    }

    // DepthStencil テクスチャを作成（既存の関数をそのまま使う）
    ID3D12Resource* depthStencilResource =
        CreateDepthStencilTextureResource(
            device.Get(),
            WinApp::kClientWidth,
            WinApp::kClientHeight
        );
    depthStencilResource_ = depthStencilResource; // ここは今まで通りの型でOK

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    device->CreateDepthStencilView(
        depthStencilResource_.Get(),
        &dsvDesc,
        dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart()
    );
}


void DirectXCommon::InitializeDescriptorHeaps()
{
    // DescriptorSize を取っておく
    descriptorSizeRTV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeSRV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    descriptorSizeDSV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // RTV用（2個）
    rtvDescriptorHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        2,
        false
    );

    // SRV用（128個）
    srvDescriptorHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        kMaxSRVCount,
        true
    );

    // DSV用（1個）
    dsvDescriptorHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        1,
        false
    );
}


ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = heapType;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ComPtr<ID3D12DescriptorHeap> heap;
    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    assert(SUCCEEDED(hr));

    return heap; // ComPtr は値返しでOK（参照カウントも保持される）
}

void DirectXCommon::InitializeRenderTargetView() {

    // ① RTV ディスクリプタヒープを作成（2個）
    rtvDescriptorHeap_ =
        CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

    descriptorSizeRTV_ =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // ② バックバッファをスワップチェーンから引っ張ってくる
    for (UINT i = 0; i < 2; i++) {

        HRESULT hr = swapChain_->GetBuffer(
            i,
            IID_PPV_ARGS(swapChainResources_[i].GetAddressOf())
        );
        assert(SUCCEEDED(hr));

        // ③ RTV の CPU ハンドルを取得
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

        rtvHandle.ptr += static_cast<SIZE_T>(i) * descriptorSizeRTV_;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;    // ← SRGB にする
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;

        // ④ RTV を作成
        device->CreateRenderTargetView(
            swapChainResources_[i].Get(),
            &rtvDesc,
            rtvHandle
        );
    }
}

void DirectXCommon::InitializeDepthStencilView()
{
    // DepthStencil Textureをウィンドウのサイズで作成
    ID3D12Resource* depthStencilResource =
        CreateDepthStencilTextureResource(
            device.Get(),                 // ← main の device を device_ に変えるだけ
            WinApp::kClientWidth,
            WinApp::kClientHeight);

    // DSVの設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;           // Format。基本的にはResourceに合わせる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;    // 2dTexture

    // DSVHeapの先頭にDSVをつくる
    device->CreateDepthStencilView(
        depthStencilResource,
        &dsvDesc,
        dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::InitializeFence()
{
    HRESULT hr = S_OK;

    // フェンス作成（main のコードそのまま）
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    fenceValue_ = 0;

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeScissorRect()
{
    // --- main のコードをそのまま移植 ---

    // シザー矩形
    scissorRect_.left = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.top = 0;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void DirectXCommon::InitializeDXC()
{
    HRESULT hr = S_OK;

    // --- main.cpp の内容をそのまま移植 ---

    // DXCの生成
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // includeHandlerを生成
    hr = dxcUtils_->CreateDefaultIncludeHandler(&dxcIncludeHandler_);
    assert(SUCCEEDED(hr));
}

//void DirectXCommon::InitializeImGui()
//{
//    // --- ImGui初期化（main のコードをそのままメンバに合わせただけ） ---
//    IMGUI_CHECKVERSION();
//    ImGui::CreateContext();
//    ImGui::StyleColorsDark();
//
//    // WinApp は Initialize() で渡して保持している前提
//    //ImGui_ImplWin32_Init(winApp->GetHwnd());
//
//    // main では swapChainDesc.BufferCount を使っていたので、
//    // ここでは swapChain_ から同じ値を取ってくる
//    DXGI_SWAP_CHAIN_DESC swapDesc{};
//    swapChain_->GetDesc(&swapDesc);
//
//    ImGui_ImplDX12_Init(
//        device.Get(),                      // device
//        swapDesc.BufferCount,              // swapChainDesc.BufferCount 相当
//        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,   // main と同じフォーマット
//        srvDescriptorHeap_.Get(),          // srvDescriptorHeap
//        srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
//        srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart());
//
//    ImGui::GetIO().IniFilename = nullptr;
//
//}

void DirectXCommon::PreDraw()
{
    assert(commandList_);
    assert(swapChain_);
    assert(rtvDescriptorHeap_);
    assert(dsvDescriptorHeap_);
    assert(srvDescriptorHeap_);

    // 1) バックバッファの番号取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // 2) Present -> RenderTarget にリソースバリア
    D3D12_RESOURCE_BARRIER barrierBegin{};
    barrierBegin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierBegin.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierBegin.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    barrierBegin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrierBegin.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierBegin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrierBegin);

    // 3) 今のバックバッファ用の RTV ハンドルを計算
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(backBufferIndex) * descriptorSizeRTV_;

    // 4) DSV ハンドル取得
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 5) 画面全体の色をクリア
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; // ← main と同じ色
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 6) 画面全体の深度をクリア
    commandList_->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,            // 深度クリア値
        0,               // ステンシル値
        0, nullptr);

    // 7) 描画先の RTV / DSV を指定
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // 8) SRV 用ディスクリプタヒープをセット
    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // 9) ビューポート領域の設定
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(WinApp::kClientWidth);
    viewport.Height = static_cast<float>(WinApp::kClientHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList_->RSSetViewports(1, &viewport);

    // 10) シザー矩形の設定
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PostDraw()
{
    assert(commandList_);
    assert(commandQueue_);
    assert(swapChain_);
    assert(fence_);

    HRESULT hr = S_OK;

    // 1. バックバッファの番号取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // 2. RenderTarget -> Present にリソースバリア
    D3D12_RESOURCE_BARRIER barrierEnd{};
    barrierEnd.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierEnd.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierEnd.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    barrierEnd.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierEnd.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrierEnd.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrierEnd);

    // 3. コマンドリストを閉じる
    hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    // 4. コマンドリストの実行（ExecuteCommandLists）
    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, cmdLists);

    // 5. 画面の交換（Present）
    hr = swapChain_->Present(1, 0);
    assert(SUCCEEDED(hr));

    // 6. コマンドの実行完了を待つ（資料スライドそのまま）
    commandQueue_->Signal(fence_, ++fenceValue_);

    if (fence_->GetCompletedValue() != fenceValue_) {
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fence_->SetEventOnCompletion(fenceValue_, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }

    // 7. 固定FPS制御
    UpdateFixFPS();

    // 8. 次フレームのためにリセット
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}


void DirectXCommon::InitializeFixFPS()
{
	// 固定FPS制御の初期化
	reference_ = std::chrono::high_resolution_clock::now();
}

void DirectXCommon::UpdateFixFPS()
{
	// 固定FPS制御
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));    // 60FPS
    // 
    const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

    // 現在時間を取得する
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    // 前回記録からの経過時間を取得する
    std::chrono::microseconds elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    if (elapsed < kMinCheckTime) {
        // 1/60秒経過するまで微小なスリープを繰り返す
        while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
            // 1マイクロ秒スリープ
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

    // 現在の時間を記録する
    reference_ = std::chrono::steady_clock::now();
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile)
{
    HRESULT hr = S_OK;

    // 開始ログ（とりあえず簡単な文字列だけ）
    Logger::Log("Begin CompileShader");


    // 1. hlslファイル読み込み
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
    hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr));

    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    // 2. コンパイル引数
    LPCWSTR arguments[] = {
        filePath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi", L"-Qembed_debug",
        L"-Od",
        L"-Zpr",
    };

    // 3. コンパイル
    Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
    hr = dxcCompiler_->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        dxcIncludeHandler_.Get(),
        IID_PPV_ARGS(&shaderResult));
    assert(SUCCEEDED(hr));

    // 4. エラーチェック
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
    hr = shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (SUCCEEDED(hr) && shaderError && shaderError->GetStringLength() != 0) {
        // UTF-8 のエラーメッセージをそのまま出す
        Logger::Log(shaderError->GetStringPointer());
        assert(false);
    }

    // 5. バイナリ取得
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    Logger::Log("Compile Succeeded");

    return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    // ヒープ設定（アップロード用）
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    // リソース設定（バッファ）
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 作成先
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;

    Logger::Log("Creating buffer resource...");

    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource)
    );

    if (FAILED(hr)) {
        Logger::Log("Failed to create buffer resource.");
        assert(false);                // デバッグ中はここで止める
        return nullptr;               // ComPtr が空のまま返る
    }

    Logger::Log("Buffer resource created successfully.");
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(
    const DirectX::TexMetadata& metadata)
{
    // metadaraをもとにResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = static_cast<UINT>(metadata.width);      // Textureの幅
    resourceDesc.Height = static_cast<UINT>(metadata.height);     // Textureの高さ
    resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);// mipmapの数
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);// 奥行きor配列数
    resourceDesc.Format = metadata.format;                        // TextureのFormat
    resourceDesc.SampleDesc.Count = 1;                                      // サンプリングカウント。1固定
    resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);

    // 利用するHeapの設定（書き戻し可能なCPUアクセス用）
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;                 // 細かい設定を行う
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;     // WriteBackポリシー
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;                   // プロセッサの近く

    // リソース作成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;

    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource)
    );

    if (FAILED(hr)) {
        Logger::Log("Failed to create texture resource.");
        assert(false); // デバッグ中は止める
        return nullptr; // ComPtr が空のまま返る
    }

    Logger::Log("Texture resource created successfully.");
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(
    const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
    const DirectX::ScratchImage& mipImages)
{
    // Meta情報を取得
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

    // 全MipMapについて
    for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {

        // MipMapLevelを指定して各Imageを取得
        const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);

        // Textureに転送
        HRESULT hr = texture->WriteToSubresource(
            static_cast<UINT>(mipLevel),
            nullptr,
            img->pixels,
            static_cast<UINT>(img->rowPitch),
            static_cast<UINT>(img->slicePitch));
        assert(SUCCEEDED(hr));
    }

    // ★ 今は中間リソースを使っていないので、空の ComPtr を返す
    return Microsoft::WRL::ComPtr<ID3D12Resource>{};
    // もしくは単に  return {};  でもOK
}




