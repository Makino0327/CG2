#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <string>
#include <vector>
#include "../winapp/WinApp.h"
#include <dxcapi.h>
#include <chrono>

#include "../../math/Math.h"

#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../../externals/DirectXTex/d3dx12.h"

class DirectXCommon
{
public:
    /// CPU ディスクリプタハンドルを取得
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    /// GPU ディスクリプタハンドルを取得
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        const DirectX::TexMetadata& metadata);

    [[nodiscard]]
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    // ComputeShader で書き込み可能な DefaultHeap のバッファを作る
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBufferResource(size_t sizeInBytes);

    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

    void Initialize(WinApp* winApp);
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeDepthBuffer();
    void InitializeDescriptorHeaps();
    void InitializeRenderTargetView();
    void InitializeDepthStencilView();
    void InitializeFence();
    void InitializeScissorRect();
    void InitializeDXC();
    void InitializeImGui();

    void PreDraw();
    void PostDraw();

    void InitializeFixFPS();
    void UpdateFixFPS();

    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    // ImGui の DirectX12 backend がテクスチャアップロードに使うコマンドキューを渡す
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const {
        return srvDescriptorHeap_.Get();
    }

    uint32_t GetDescriptorSizeSRV() const { return descriptorSizeSRV_; }

    uint32_t GetBackBufferCount() const {
        return static_cast<uint32_t>(swapChainResources_.size());
    }

    DXGI_FORMAT GetRTVFormat() const {
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle() {
        return GetSRVCPUDescriptorHandle(0);
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() {
        return GetSRVGPUDescriptorHandle(0);
    }

    size_t GetSwapChainResourcesNum() const {
        return swapChainResources_.size();
    }

    ~DirectXCommon();

    static const uint32_t kMaxSRVCount;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors,
        bool shaderVisible);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Vector4& clearColor);
    ID3D12Resource* GetCurrentBackBufferResource() const {
        return swapChainResources_[swapChain_->GetCurrentBackBufferIndex()].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTVHandle() const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(swapChain_->GetCurrentBackBufferIndex()) * descriptorSizeRTV_;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const {
        return dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    UINT descriptorSizeDSV_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    UINT64 fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    D3D12_RECT scissorRect_{};

    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler_;

    WinApp* winApp = nullptr;

    UINT descriptorSizeRTV_ = 0;
    UINT descriptorSizeSRV_ = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;

    std::chrono::steady_clock::time_point reference_;
};
