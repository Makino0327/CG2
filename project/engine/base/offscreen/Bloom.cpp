#include "Bloom.h"
#include <cassert>

namespace {
    // 各パスの入力画像を読み取り状態、出力画像を描画状態へ切り替える。
    void TransitionBloom(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1, &barrier);
    }
}

void Bloom::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    const uint32_t width = (WinApp::kClientWidth+1)/2;
    const uint32_t height = (WinApp::kClientHeight+1)/2;
    brightTexture_ = std::make_unique<RenderTexture>();
    blurTexture_ = std::make_unique<RenderTexture>();
    // 小さな光の値がぼかし中に失われないよう、中間画像は浮動小数点で保持する。
    brightTexture_->Initialize(dxCommon_, srvManager_, width, height,
        DXGI_FORMAT_R16G16B16A16_FLOAT, { 0,0,0,1 });
    blurTexture_->Initialize(dxCommon_, srvManager_, width, height,
        DXGI_FORMAT_R16G16B16A16_FLOAT, { 0,0,0,1 });
    CreatePipeline();
}

void Bloom::CreatePipeline() {
    D3D12_DESCRIPTOR_RANGE ranges[3]{};
    D3D12_ROOT_PARAMETER roots[4]{};
    for (uint32_t index = 0; index < 3; ++index) {
        // 元画像・ぼかした光・ぼかす前の光を、それぞれt0・t1・t2へ渡す。
        ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[index].NumDescriptors = 1;
        ranges[index].BaseShaderRegister = index;
        ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        roots[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        roots[index].DescriptorTable = { 1,&ranges[index] };
        roots[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    // 定数をコマンドへ直接保存し、横と縦のパスで方向が上書きされるのを防ぐ。
    static_assert(sizeof(Parameters) == 32);
    roots[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    roots[3].Constants.Num32BitValues = sizeof(Parameters)/sizeof(uint32_t);
    roots[3].Constants.ShaderRegister = 0;
    roots[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    signature.NumParameters = _countof(roots);
    signature.pParameters = roots;
    signature.NumStaticSamplers = 1;
    signature.pStaticSamplers = &sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    assert(SUCCEEDED(hr));
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    auto vs = dxCommon_->CompileShader(L"Resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.VS = { vs->GetBufferPointer(),vs->GetBufferSize() };
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.SampleDesc.Count = 1;
    const auto create = [&](const wchar_t* path, DXGI_FORMAT format,
        Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline) {
        auto ps = dxCommon_->CompileShader(path, L"ps_6_0");
        desc.PS = { ps->GetBufferPointer(),ps->GetBufferSize() };
        desc.RTVFormats[0] = format;
        const HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
            &desc, IID_PPV_ARGS(pipeline.GetAddressOf()));
        assert(SUCCEEDED(result));
    };
    create(L"Resources/shaders/BloomExtract.PS.hlsl", DXGI_FORMAT_R16G16B16A16_FLOAT, extractPipeline_);
    create(L"Resources/shaders/BloomBlur.PS.hlsl", DXGI_FORMAT_R16G16B16A16_FLOAT, blurPipeline_);
    create(L"Resources/shaders/BloomComposite.PS.hlsl", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, compositePipeline_);
}

void Bloom::Draw(uint32_t inputSrvIndex, uint32_t emissionSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRTV) {
    ID3D12GraphicsCommandList* list = dxCommon_->GetCommandList();
    const uint32_t width = (WinApp::kClientWidth+1)/2;
    const uint32_t height = (WinApp::kClientHeight+1)/2;
    const auto draw = [&](ID3D12PipelineState* pipeline, uint32_t input, uint32_t glow,
        D3D12_CPU_DESCRIPTOR_HANDLE output, uint32_t targetWidth, uint32_t targetHeight, Vector2 direction) {
        list->OMSetRenderTargets(1, &output, FALSE, nullptr);
        const float clearColor[4] = { 0,0,0,1 };
        list->ClearRenderTargetView(output, clearColor, 0, nullptr);
        const D3D12_VIEWPORT viewport{ 0,0,static_cast<float>(targetWidth),static_cast<float>(targetHeight),0,1 };
        const D3D12_RECT scissor{ 0,0,static_cast<LONG>(targetWidth),static_cast<LONG>(targetHeight) };
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
        list->SetGraphicsRootSignature(rootSignature_.Get());
        list->SetPipelineState(pipeline);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
        list->SetDescriptorHeaps(_countof(heaps), heaps);
        srvManager_->SetGraphicsRootDescriptorTable(0, input);
        srvManager_->SetGraphicsRootDescriptorTable(1, glow);
        // 光の芯には、半分の解像度へ縮小していない発光画像を使う。
        srvManager_->SetGraphicsRootDescriptorTable(2, emissionSrvIndex);
        Parameters passParameters = parameters_;
        passParameters.direction = direction;
        list->SetGraphicsRoot32BitConstants(3, sizeof(Parameters)/sizeof(uint32_t), &passParameters, 0);
        list->DrawInstanced(3, 1, 0, 0);
    };

    // 全画面ではなく、個別に指定された発光用画像だけを半分の解像度へ集める。
    draw(extractPipeline_.Get(), emissionSrvIndex, emissionSrvIndex,
        brightTexture_->GetRTVHandle(), width, height, { 0,0 });
    TransitionBloom(list, brightTexture_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    // 横方向にぼかし、次に縦方向へぼかす。画面全体をぼかさず光だけを広げる。
    draw(blurPipeline_.Get(), brightTexture_->GetSRVIndex(), brightTexture_->GetSRVIndex(),
        blurTexture_->GetRTVHandle(), width, height, { 1,0 });
    TransitionBloom(list, blurTexture_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionBloom(list, brightTexture_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    draw(blurPipeline_.Get(), blurTexture_->GetSRVIndex(), blurTexture_->GetSRVIndex(),
        brightTexture_->GetRTVHandle(), width, height, { 0,1 });
    TransitionBloom(list, brightTexture_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    // 元の画面へ光を足し、背景の細部はそのまま残す。
    draw(compositePipeline_.Get(), inputSrvIndex, brightTexture_->GetSRVIndex(),
        outputRTV, WinApp::kClientWidth, WinApp::kClientHeight, { 0,0 });
    // 次のフレームも同じ状態から始められるよう、中間画像を描画状態へ戻す。
    TransitionBloom(list, brightTexture_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionBloom(list, blurTexture_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
}
