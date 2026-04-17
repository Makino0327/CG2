#include "RenderTexture.h"

void RenderTexture::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    format_ = format;
    clearColor_ = clearColor;

    resource_ = dxCommon_->CreateRenderTextureResource(
        width,
        height,
        format_,
        clearColor_);

    rtvDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        1,
        false);

    rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    dxCommon_->GetDevice()->CreateRenderTargetView(
        resource_.Get(),
        &rtvDesc,
        rtvHandle_);

    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVForRenderTexture(
        srvIndex_,
        resource_.Get(),
        format_);
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTexture::GetSRVCPUHandle() const
{
    return srvManager_->GetCPUDescriptorHandle(srvIndex_);
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTexture::GetSRVGPUHandle() const
{
    return srvManager_->GetGPUDescriptorHandle(srvIndex_);
}

void RenderTexture::PreDraw(
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
    ID3D12DescriptorHeap* srvDescriptorHeap) const
{
    assert(commandList);

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(WinApp::kClientWidth);
    viewport.Height = static_cast<float>(WinApp::kClientHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = WinApp::kClientWidth;
    scissorRect.bottom = WinApp::kClientHeight;

    commandList->OMSetRenderTargets(1, &rtvHandle_, FALSE, &dsvHandle);

    float clearColor[] = {
        clearColor_.x,
        clearColor_.y,
        clearColor_.z,
        clearColor_.w
    };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}
