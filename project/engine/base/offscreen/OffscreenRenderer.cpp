#include "OffscreenRenderer.h"
#include <cassert>

void OffscreenRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    renderTexture_ = std::make_unique<RenderTexture>();
    renderTexture_->Initialize(
        dxCommon_,
        srvManager_,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        Vector4(1.0f, 0.0f, 0.0f, 1.0f));

    depthSrvIndex_ = srvManager_->Allocate(); // DepthTexture用のSRV番号を確保する
    srvManager_->CreateSRVForDepthTexture(
        depthSrvIndex_,
        dxCommon_->GetDepthStencilResource()); // DepthBufferをPixelShaderから読めるようにする

    CreateRootSignature();
    CreateGraphicsPipelineState();
}

void OffscreenRenderer::PreDrawScene()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);
    assert(renderTexture_);

    ID3D12Resource* backBufferResource = dxCommon_->GetCurrentBackBufferResource();

    D3D12_RESOURCE_BARRIER barrierBegin{};
    barrierBegin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierBegin.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierBegin.Transition.pResource = backBufferResource;
    barrierBegin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrierBegin.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierBegin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierBegin);

    renderTexture_->PreDraw(
        commandList,
        dxCommon_->GetDSVHandle(),
        srvManager_->GetDescriptorHeap());
}

void OffscreenRenderer::DrawToBackBuffer()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);
    assert(renderTexture_);

    ID3D12Resource* depthResource = dxCommon_->GetDepthStencilResource(); // // Outlineで読むDepthResource

    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTVHandle = dxCommon_->GetCurrentBackBufferRTVHandle();

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

    if (postEffectType_ == PostEffectType::DepthOutline) {
        D3D12_RESOURCE_BARRIER depthBarrier{};
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        depthBarrier.Transition.pResource = depthResource;
        depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &depthBarrier); // // Depthを書き込み用から読み込み用へ切り替える
    }

    commandList->OMSetRenderTargets(1, &backBufferRTVHandle, FALSE, nullptr);
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    switch (postEffectType_) {
    case PostEffectType::Copy:
        commandList->SetPipelineState(copyPipelineState_.Get());
        break;

    case PostEffectType::Grayscale:
        commandList->SetPipelineState(grayscalePipelineState_.Get());
        break;

    case PostEffectType::Sepia:
        commandList->SetPipelineState(sepiaPipelineState_.Get());
        break;
    case PostEffectType::Vignette:
        commandList->SetPipelineState(vignettePipelineState_.Get());
        break;
    case PostEffectType::BoxFilter:
        // ボックスフィルタ用のパイプラインステートを設定する
        commandList->SetPipelineState(boxFilterPipelineState_.Get());
        break;
    case PostEffectType::GaussianFilter:
        // ガウシアンフィルタ用のパイプラインステートを設定する
        commandList->SetPipelineState(gaussianFilterPipelineState_.Get());
        break;
    case PostEffectType::DepthOutline:
        commandList->SetPipelineState(depthOutlinePipelineState_.Get()); // DepthベースのOutlineを使う
        break;


    }

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    srvManager_->SetGraphicsRootDescriptorTable(0, renderTexture_->GetSRVIndex()); // t0にカラーを渡す
    srvManager_->SetGraphicsRootDescriptorTable(1, depthSrvIndex_); // t1にDepthを渡す

    if (postEffectType_ == PostEffectType::DepthOutline) {
        D3D12_RESOURCE_BARRIER depthBarrier{};
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        depthBarrier.Transition.pResource = depthResource;
        depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &depthBarrier); // // 次フレームでDepthを書ける状態に戻す
    }


    commandList->DrawInstanced(3, 1, 0, 0);
}


void OffscreenRenderer::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE textureRange{};
    textureRange.BaseShaderRegister = 0;
    textureRange.NumDescriptors = 1;
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE depthRange{};
    depthRange.BaseShaderRegister = 1;
    depthRange.NumDescriptors = 1;
    depthRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    depthRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &textureRange;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &depthRange;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[2]{};

    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // // Depthは補間しない
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[1].ShaderRegister = 1;
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}


void OffscreenRenderer::CreateGraphicsPipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    auto vertexShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/CopyImage.VS.hlsl",
        L"vs_6_0");
    auto copyPixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/CopyImage.PS.hlsl",
        L"ps_6_0");

    auto grayscalePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Grayscale.PS.hlsl",
        L"ps_6_0");

    auto sepiaPixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Sepia.PS.hlsl",
        L"ps_6_0");
    auto vignettePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Vignette.PS.hlsl",
        L"ps_6_0");
    auto boxFilterPixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/BoxFilter.PS.hlsl",
        L"ps_6_0");
    auto gaussianFilterPixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/GaussianFilter.PS.hlsl",
        L"ps_6_0");
    auto depthOutlinePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/DepthBasedOutline.PS.hlsl",
        L"ps_6_0");

    D3D12_INPUT_LAYOUT_DESC inputLayout{};
    inputLayout.pInputElementDescs = nullptr;
    inputLayout.NumElements = 0;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false;
    depthStencilDesc.StencilEnable = false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = inputLayout;
    desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    desc.BlendState = blendDesc;
    desc.RasterizerState = rasterizerDesc;
    desc.DepthStencilState = depthStencilDesc;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    HRESULT hr = S_OK;

    desc.PS = {
        copyPixelShaderBlob->GetBufferPointer(),
        copyPixelShaderBlob->GetBufferSize()
    };
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(copyPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    desc.PS = {
        grayscalePixelShaderBlob->GetBufferPointer(),
        grayscalePixelShaderBlob->GetBufferSize()
    };
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(grayscalePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    desc.PS = {
        sepiaPixelShaderBlob->GetBufferPointer(),
        sepiaPixelShaderBlob->GetBufferSize()
    };
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(sepiaPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // ヴィネッティング用のピクセルシェーダを設定する
    desc.PS = {
        vignettePixelShaderBlob->GetBufferPointer(),
        vignettePixelShaderBlob->GetBufferSize()
    };

    // ヴィネッティング用のパイプラインステートを作成する
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(vignettePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
    // ボックスフィルタ用のピクセルシェーダを設定する
    desc.PS = {
        boxFilterPixelShaderBlob->GetBufferPointer(),
        boxFilterPixelShaderBlob->GetBufferSize()
    };

    // ボックスフィルタ用のパイプラインステートを作成する
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(boxFilterPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // ガウシアンフィルタ用のピクセルシェーダを設定する
    desc.PS = {
        gaussianFilterPixelShaderBlob->GetBufferPointer(),
        gaussianFilterPixelShaderBlob->GetBufferSize()
    };

    // ガウシアンフィルタ用のパイプラインステートを作成する
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(gaussianFilterPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
    // アウトライン用のピクセルシェーダ
    desc.PS = {
    depthOutlinePixelShaderBlob->GetBufferPointer(),
    depthOutlinePixelShaderBlob->GetBufferSize()
    };

    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(depthOutlinePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

}

