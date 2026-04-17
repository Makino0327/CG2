#pragma once
#include "../DirectX/DirectXCommon.h"
#include "../srv/SrvManager.h"
#include "../../math/Math.h"

class RenderTexture {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Vector4& clearColor);

    ID3D12Resource* GetResource() const { return resource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const;
    uint32_t GetSRVIndex() const { return srvIndex_; }
    const Vector4& GetClearColor() const { return clearColor_; }
    DXGI_FORMAT GetFormat() const { return format_; }
    void PreDraw(
        ID3D12GraphicsCommandList* commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
        ID3D12DescriptorHeap* srvDescriptorHeap) const;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};

    uint32_t srvIndex_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    Vector4 clearColor_{};
};
