#include "DirectXCommon.h"
#include <cassert>
#include <format>
#include "Logger.h" 

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;

void DirectXCommon::Initialize()
{
}

void DirectXCommon::InitializeDevice()
{
    HRESULT hr;

#ifdef _DEBUG
    // デバッグレイヤー有効化
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
        Logger::Log("[DebugLayer] Enabled GPU-based validation");
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
