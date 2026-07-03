#include "OffscreenRenderer.h"
#include <cassert>
#include "../../2d/texture/TextureManager.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

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

    depthSrvIndex_ = srvManager_->Allocate(); // DepthTexture逕ｨ縺ｮSRV逡ｪ蜿ｷ繧堤｢ｺ菫昴☆繧・
    srvManager_->CreateSRVForDepthTexture(
        depthSrvIndex_,
        dxCommon_->GetDepthStencilResource()); // DepthBuffer繧単ixelShader縺九ｉ隱ｭ繧√ｋ繧医≧縺ｫ縺吶ｋ

    CreateRootSignature();
    CreateGraphicsPipelineState();

    // 繝ｩ繧ｸ繧｢繝ｫ繝悶Λ繝ｼ逕ｨ縺ｮ螳壽焚繝舌ャ繝輔ぃ繧剃ｽ懈・縺吶ｋ
    radialBlurResource_ = dxCommon_->CreateBufferResource(sizeof(RadialBlurData));
    radialBlurResource_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurData_));

    // 蛻晄悄蛟､縺ｨ縺励※逕ｻ髱｢荳ｭ螟ｮ繧剃ｸｭ蠢・↓縺励※蠑ｱ繧√↓縺ｼ縺九☆
    radialBlurData_->center = { 0.5f, 0.5f };
    radialBlurData_->blurWidth = 0.01f;
    radialBlurData_->padding = 0.0f;

    TextureManager* textureManager = TextureManager::GetInstance();

    // 繝・ぅ繧ｾ繝ｫ繝也畑縺ｮ繝槭せ繧ｯ繝・け繧ｹ繝√Ε繧定ｪｭ縺ｿ霎ｼ繧
    textureManager->LoadTexture("Resources/noise0.png");
    textureManager->LoadTexture("Resources/noise1.png");

    dissolveMaskSrvIndex0_ = textureManager->GetSrvIndex("Resources/noise0.png");
    dissolveMaskSrvIndex1_ = textureManager->GetSrvIndex("Resources/noise1.png");

    // 繝・ぅ繧ｾ繝ｫ繝也畑縺ｮ螳壽焚繝舌ャ繝輔ぃ繧剃ｽ懈・縺吶ｋ
    dissolveResource_ = dxCommon_->CreateBufferResource(sizeof(DissolveData));
    dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));

    dissolveData_->threshold = 0.0f;
    dissolveData_->edgeWidth = 0.03f;
    dissolveData_->padding = { 0.0f, 0.0f };
    dissolveData_->edgeColor = { 1.0f, 0.4f, 0.3f, 1.0f };

    // 繝ｩ繝ｳ繝繝繝弱う繧ｺ逕ｨ縺ｮ螳壽焚繝舌ャ繝輔ぃ繧剃ｽ懈・縺吶ｋ
    randomNoiseResource_ = dxCommon_->CreateBufferResource(sizeof(RandomNoiseData));
    randomNoiseResource_->Map(0, nullptr, reinterpret_cast<void**>(&randomNoiseData_));

    randomNoiseData_->intensity = 0.3f;
    randomNoiseData_->time = 0.0f;
    randomNoiseData_->speed = 1.0f;
    randomNoiseData_->padding = 0.0f;

    // 衝撃波歪み用の定数バッファを作る
    shockwaveResource_ = dxCommon_->CreateBufferResource(sizeof(ShockwaveData));
    shockwaveResource_->Map(0, nullptr, reinterpret_cast<void**>(&shockwaveData_));

    shockwaveData_->center = { 0.5f, 0.5f };
    shockwaveData_->radius = 0.02f;
    shockwaveData_->thickness = 0.035f;
    shockwaveData_->strength = 0.0f;
    shockwaveData_->progress = 1.0f;
    shockwaveData_->aspectRatio = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
    shockwaveData_->whiteWave = shockwaveWhiteWaveEnabled_ ? 1.0f : 0.0f;


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

    ID3D12Resource* depthResource = dxCommon_->GetDepthStencilResource(); // // Outline縺ｧ隱ｭ繧DepthResource

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
        commandList->ResourceBarrier(1, &depthBarrier); // // Depth繧呈嶌縺崎ｾｼ縺ｿ逕ｨ縺九ｉ隱ｭ縺ｿ霎ｼ縺ｿ逕ｨ縺ｸ蛻・ｊ譖ｿ縺医ｋ
    }

    // シーン描画先をRenderTargetからPixelShaderの読み込み用へ切り替える
    D3D12_RESOURCE_BARRIER renderTextureToRead{};
    renderTextureToRead.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    renderTextureToRead.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    renderTextureToRead.Transition.pResource = renderTexture_->GetResource();
    renderTextureToRead.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    renderTextureToRead.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    renderTextureToRead.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &renderTextureToRead);

    commandList->OMSetRenderTargets(1, &backBufferRTVHandle, FALSE, nullptr);
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(backBufferRTVHandle, clearColor, 0, nullptr);

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
        // 繝懊ャ繧ｯ繧ｹ繝輔ぅ繝ｫ繧ｿ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ險ｭ螳壹☆繧・
        commandList->SetPipelineState(boxFilterPipelineState_.Get());
        break;
    case PostEffectType::GaussianFilter:
        // 繧ｬ繧ｦ繧ｷ繧｢繝ｳ繝輔ぅ繝ｫ繧ｿ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ險ｭ螳壹☆繧・
        commandList->SetPipelineState(gaussianFilterPipelineState_.Get());
        break;
    case PostEffectType::DepthOutline:
        commandList->SetPipelineState(depthOutlinePipelineState_.Get()); // Depth繝吶・繧ｹ縺ｮOutline繧剃ｽｿ縺・
        break;

    case PostEffectType::RadialBlur:
        // 繝ｩ繧ｸ繧｢繝ｫ繝悶Λ繝ｼ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ險ｭ螳壹☆繧・
        commandList->SetPipelineState(radialBlurPipelineState_.Get());
        break;
    case PostEffectType::Dissolve:
        // 繝・ぅ繧ｾ繝ｫ繝也畑縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ險ｭ螳壹☆繧・
        commandList->SetPipelineState(dissolvePipelineState_.Get());
        break;
    case PostEffectType::RandomNoise:
        commandList->SetPipelineState(randomNoisePipelineState_.Get());
        break;
    case PostEffectType::Shockwave:
        commandList->SetPipelineState(shockwavePipelineState_.Get());
        break;

    }

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    srvManager_->SetGraphicsRootDescriptorTable(0, renderTexture_->GetSRVIndex()); // t0
    srvManager_->SetGraphicsRootDescriptorTable(1, depthSrvIndex_); // t1

    uint32_t maskSrvIndex = dissolveMaskType_ == 0 ? dissolveMaskSrvIndex0_ : dissolveMaskSrvIndex1_;
    srvManager_->SetGraphicsRootDescriptorTable(2, maskSrvIndex); // t2

    commandList->SetGraphicsRootConstantBufferView(
        3,
        radialBlurResource_->GetGPUVirtualAddress()); // b0

    commandList->SetGraphicsRootConstantBufferView(
        4,
        dissolveResource_->GetGPUVirtualAddress()); // b4

    commandList->SetGraphicsRootConstantBufferView(
        5,
        randomNoiseResource_->GetGPUVirtualAddress()); // b2

    commandList->SetGraphicsRootConstantBufferView(
        6,
        shockwaveResource_->GetGPUVirtualAddress()); // b3

    if (postEffectType_ == PostEffectType::DepthOutline) {
        D3D12_RESOURCE_BARRIER depthBarrier{};
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        depthBarrier.Transition.pResource = depthResource;
        depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &depthBarrier); // // 谺｡繝輔Ξ繝ｼ繝縺ｧDepth繧呈嶌縺代ｋ迥ｶ諷九↓謌ｻ縺・
    }


    commandList->DrawInstanced(3, 1, 0, 0);

    // 画面へのコピー完了後、次フレームのシーン描画用へ戻す
    D3D12_RESOURCE_BARRIER renderTextureToWrite{};
    renderTextureToWrite.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    renderTextureToWrite.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    renderTextureToWrite.Transition.pResource = renderTexture_->GetResource();
    renderTextureToWrite.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    renderTextureToWrite.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    renderTextureToWrite.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &renderTextureToWrite);
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

    D3D12_DESCRIPTOR_RANGE maskRange{};
    maskRange.BaseShaderRegister = 2;
    maskRange.NumDescriptors = 1;
    maskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    maskRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[7]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &textureRange;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &depthRange;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &maskRange;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 0;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 1;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister = 2;

    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 3;


    D3D12_STATIC_SAMPLER_DESC staticSamplers[2]{};

    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // // Depth縺ｯ陬憺俣縺励↑縺・
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
    auto radialBlurPixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/RadialBlur.PS.hlsl",
        L"ps_6_0");
    auto dissolvePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Dissolve.PS.hlsl",
        L"ps_6_0");
    auto randomNoisePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/RandomNoise.PS.hlsl",
        L"ps_6_0");
    auto shockwavePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Shockwave.PS.hlsl",
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

    // 繝ｴ繧｣繝阪ャ繝・ぅ繝ｳ繧ｰ逕ｨ縺ｮ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繧定ｨｭ螳壹☆繧・
    desc.PS = {
        vignettePixelShaderBlob->GetBufferPointer(),
        vignettePixelShaderBlob->GetBufferSize()
    };

    // 繝ｴ繧｣繝阪ャ繝・ぅ繝ｳ繧ｰ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ菴懈・縺吶ｋ
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(vignettePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
    // 繝懊ャ繧ｯ繧ｹ繝輔ぅ繝ｫ繧ｿ逕ｨ縺ｮ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繧定ｨｭ螳壹☆繧・
    desc.PS = {
        boxFilterPixelShaderBlob->GetBufferPointer(),
        boxFilterPixelShaderBlob->GetBufferSize()
    };

    // 繝懊ャ繧ｯ繧ｹ繝輔ぅ繝ｫ繧ｿ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ菴懈・縺吶ｋ
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(boxFilterPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // 繧ｬ繧ｦ繧ｷ繧｢繝ｳ繝輔ぅ繝ｫ繧ｿ逕ｨ縺ｮ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繧定ｨｭ螳壹☆繧・
    desc.PS = {
        gaussianFilterPixelShaderBlob->GetBufferPointer(),
        gaussianFilterPixelShaderBlob->GetBufferSize()
    };

    // 繧ｬ繧ｦ繧ｷ繧｢繝ｳ繝輔ぅ繝ｫ繧ｿ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ菴懈・縺吶ｋ
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(gaussianFilterPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
    // 繧｢繧ｦ繝医Λ繧､繝ｳ逕ｨ縺ｮ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝
    desc.PS = {
    depthOutlinePixelShaderBlob->GetBufferPointer(),
    depthOutlinePixelShaderBlob->GetBufferSize()
    };

    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(depthOutlinePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // 繝ｩ繧ｸ繧｢繝ｫ繝悶Λ繝ｼ逕ｨ縺ｮ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繧定ｨｭ螳壹☆繧・
    desc.PS = {
        radialBlurPixelShaderBlob->GetBufferPointer(),
        radialBlurPixelShaderBlob->GetBufferSize()
    };

    // 繝ｩ繧ｸ繧｢繝ｫ繝悶Λ繝ｼ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ菴懈・縺吶ｋ
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(radialBlurPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // 繝・ぅ繧ｾ繝ｫ繝也畑縺ｮ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繧定ｨｭ螳壹☆繧・
    desc.PS = {
        dissolvePixelShaderBlob->GetBufferPointer(),
        dissolvePixelShaderBlob->GetBufferSize()
    };

    // 繝・ぅ繧ｾ繝ｫ繝也畑縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ菴懈・縺吶ｋ
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(dissolvePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // 繝ｩ繝ｳ繝繝繝弱う繧ｺ逕ｨ縺ｮ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繧定ｨｭ螳壹☆繧・
    desc.PS = {
        randomNoisePixelShaderBlob->GetBufferPointer(),
        randomNoisePixelShaderBlob->GetBufferSize()
    };

    // 繝ｩ繝ｳ繝繝繝弱う繧ｺ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｒ菴懈・縺吶ｋ
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(randomNoisePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // 発射時の空気の歪み用パイプラインステートを作る
    desc.PS = {
        shockwavePixelShaderBlob->GetBufferPointer(),
        shockwavePixelShaderBlob->GetBufferSize()
    };
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(shockwavePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

}
void OffscreenRenderer::Update(float deltaTime, const Vector2& mousePosition, bool isMouseRightPressed)
{
    // 繝ｩ繝ｳ繝繝繝弱う繧ｺ縺ｮ譎る俣繧帝ｲ繧√ｋ
    if (randomNoiseData_) {
        randomNoiseData_->time += deltaTime * randomNoiseData_->speed;
    }

    if (isShockwavePlaying_ && shockwaveData_) {
        shockwaveElapsedTime_ += deltaTime;

        float duration = std::max(shockwaveDuration_, 0.01f);
        float progress = std::clamp(shockwaveElapsedTime_ / duration, 0.0f, 1.0f);

        // 衝撃波を小さめに広げて、時間経過で歪みを弱くする
        shockwaveData_->progress = progress;
        shockwaveData_->radius = 0.02f + currentShockwaveMaxRadius_ * progress;
        shockwaveData_->thickness = 0.035f;
        shockwaveData_->strength = 0.014f * (1.0f - progress);
        shockwaveData_->whiteWave = shockwaveWhiteWaveEnabled_ ? 1.0f : 0.0f;

        if (progress >= 1.0f) {
            isShockwavePlaying_ = false;
            if (postEffectType_ == PostEffectType::Shockwave) {
                postEffectType_ = shockwaveReturnType_;
            }
        }
    }

    // 右ボタンを離したフレームで回転状態が残らないようにここで即座に解除する
    if (!isMouseRightPressed) {
        isGameViewRotating_ = false;
        isGameViewRotationStarted_ = false;
        gameViewMouseDelta_ = { 0.0f, 0.0f };
    }

    // 現在はマウス座標はここでは使わない
    (void)mousePosition;

    if (!isDissolvePlaying_) {
        return;
    }

    dissolveElapsedTime_ += deltaTime;

    float duration = std::max(dissolveDuration_, 0.1f);
    float progress = std::clamp(dissolveElapsedTime_ / duration, 0.0f, 1.0f);

    dissolveData_->threshold = progress;

    if (progress >= 1.0f) {
        isDissolvePlaying_ = false;
    }
}


void OffscreenRenderer::StartDissolve()
{
    dissolveElapsedTime_ = 0.0f;
    dissolveData_->threshold = 0.0f;
    isDissolvePlaying_ = true;
}
void OffscreenRenderer::StartShockwave(const Vector2& centerUV)
{
    // 通常設定の最大サイズで衝撃波を出す
    StartShockwave(centerUV, shockwaveMaxRadius_);
}

void OffscreenRenderer::StartShockwave(const Vector2& centerUV, float maxRadius)
{
    if (!shockwaveData_) {
        return;
    }

    // すでに別のエフェクトを表示している場合は、衝撃波終了後に戻す
    if (postEffectType_ != PostEffectType::Shockwave) {
        shockwaveReturnType_ = postEffectType_;
    }

    shockwaveElapsedTime_ = 0.0f;
    isShockwavePlaying_ = true;
    postEffectType_ = PostEffectType::Shockwave;

    // この1回の衝撃波だけに使う最大サイズを保存する
    currentShockwaveMaxRadius_ = std::max(maxRadius, 0.01f);

    shockwaveData_->center = {
        std::clamp(centerUV.x, 0.0f, 1.0f),
        std::clamp(centerUV.y, 0.0f, 1.0f)
    };
    shockwaveData_->radius = 0.02f;
    shockwaveData_->thickness = 0.035f;
    shockwaveData_->strength = 0.014f;
    shockwaveData_->progress = 0.0f;
    shockwaveData_->aspectRatio = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
    shockwaveData_->whiteWave = shockwaveWhiteWaveEnabled_ ? 1.0f : 0.0f;
}

void OffscreenRenderer::SetDissolveElapsedTime(float seconds)
{
    float duration = std::max(dissolveDuration_, 0.1f);

    // 迴ｾ蝨ｨ遘呈焚縺・0 譛ｪ貅繧・怙螟ｧ遘呈焚雜・∴縺ｫ縺ｪ繧峨↑縺・ｈ縺・↓縺吶ｋ
    dissolveElapsedTime_ = std::clamp(seconds, 0.0f, duration);

    // 迴ｾ蝨ｨ遘呈焚縺ｫ蜷医ｏ縺帙※ threshold 繧よ峩譁ｰ縺吶ｋ
    dissolveData_->threshold = dissolveElapsedTime_ / duration;

    // 譛蠕後∪縺ｧ陦後▲縺ｦ縺・ｌ縺ｰ蛛懈ｭ｢縲√◎繧御ｻ･螟悶・蜀咲函荳ｭ謇ｱ縺・↓縺吶ｋ
    isDissolvePlaying_ = dissolveElapsedTime_ < duration;
}

void OffscreenRenderer::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Post Effect");

    const char* items[] = {
        "Copy",
        "Grayscale",
        "Sepia",
        "Vignette",
        "BoxFilter",
        "GaussianFilter",
        "RadialBlur",
        "Dissolve",
        "RandomNoise",
        "Shockwave",
        "DepthOutline",
    };



    int current = static_cast<int>(postEffectType_);
    if (ImGui::Combo("Effect", &current, items, IM_ARRAYSIZE(items))) {
        postEffectType_ = static_cast<PostEffectType>(current);
    }

    if (postEffectType_ == PostEffectType::Dissolve) {
        const char* maskItems[] = { "noise0", "noise1" };
        if (ImGui::Combo("Mask", &dissolveMaskType_, maskItems, IM_ARRAYSIZE(maskItems))) {
        }

        ImGui::DragFloat("Duration", &dissolveDuration_, 0.1f, 0.1f, 10.0f);

        float maxSeconds = std::max(dissolveDuration_, 0.1f);
        if (ImGui::SliderFloat("Current Time", &dissolveElapsedTime_, 0.0f, maxSeconds)) {
            SetDissolveElapsedTime(dissolveElapsedTime_);
        }

        if (ImGui::Button("Start")) {
            StartDissolve();
        }

        ImGui::Text("Playing: %s", isDissolvePlaying_ ? "true" : "false");
        ImGui::Text("Threshold: %.2f", dissolveData_ ? dissolveData_->threshold : 0.0f);
    }

    if (postEffectType_ == PostEffectType::RandomNoise) {
        ImGui::DragFloat("Intensity", &randomNoiseData_->intensity, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Speed", &randomNoiseData_->speed, 0.01f, 0.0f, 10.0f);
    }

    if (postEffectType_ == PostEffectType::Shockwave) {
        ImGui::DragFloat2("Center UV", &shockwaveData_->center.x, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Duration", &shockwaveDuration_, 0.01f, 0.05f, 1.0f);
        ImGui::DragFloat("Shockwave Size", &shockwaveMaxRadius_, 0.01f, 0.05f, 0.40f);
        if (ImGui::Checkbox("White Wave", &shockwaveWhiteWaveEnabled_) && shockwaveData_) {
            // チェック変更をすぐにシェーダーへ反映する
            shockwaveData_->whiteWave = shockwaveWhiteWaveEnabled_ ? 1.0f : 0.0f;
        }
        if (ImGui::Button("Start Shockwave")) {
            StartShockwave(shockwaveData_->center);
        }
        ImGui::Text("Playing: %s", isShockwavePlaying_ ? "true" : "false");
    }


    ImGui::End();
#endif
}

void OffscreenRenderer::DrawDebugGameViewImGui()
{
#ifdef USE_IMGUI
    if (!renderTexture_) {
        return;
    }


    ImGui::Begin("Game View");

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    float aspect = 16.0f / 9.0f;

    ImVec2 imageSize = availSize;
    imageSize.y = imageSize.x / aspect;

    if (imageSize.y > availSize.y) {
        imageSize.y = availSize.y;
        imageSize.x = imageSize.y * aspect;
    }

    ImTextureID textureId =
        static_cast<ImTextureID>(renderTexture_->GetSRVGPUHandle().ptr);

    ImGui::Image(textureId, imageSize);

        // Game View 縺ｮ陦ｨ遉ｺ菴咲ｽｮ縺ｨ繧ｵ繧､繧ｺ縺縺代ｒ菫晏ｭ倥＠縺ｦ縺翫￥
    ImVec2 imageMin = ImGui::GetItemRectMin();
    ImVec2 imageSizeImGui = ImGui::GetItemRectSize();

    gameViewTopLeft_ = { imageMin.x, imageMin.y };
    gameViewSize_ = { imageSizeImGui.x, imageSizeImGui.y };

    // ImGui 縺檎｢ｺ螳壹＠縺溽洸蠖｢繧剃ｽｿ縺｣縺ｦ Game View 縺ｮ蜈･蜉帷憾諷九ｒ譖ｴ譁ｰ縺吶ｋ
    prevGameViewMousePosition_ = gameViewMousePosition_;

    // Image の hover 状態をそのまま Game View の hover として使う
    isGameViewHovered_ = ImGui::IsItemHovered();

    if (isGameViewHovered_ && imageSizeImGui.x > 0.0f && imageSizeImGui.y > 0.0f) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;

        // Game View 蜀・〒縺ｮ繝槭え繧ｹ蠎ｧ讓吶ｒ譖ｴ譁ｰ縺吶ｋ
        gameViewMousePosition_.x = mousePos.x - imageMin.x;
        gameViewMousePosition_.y = mousePos.y - imageMin.y;

        // 0.0f 縺九ｉ 1.0f 縺ｮ UV 蠎ｧ讓吶ｂ譖ｴ譁ｰ縺吶ｋ
        gameViewMouseUV_.x = gameViewMousePosition_.x / imageSizeImGui.x;
        gameViewMouseUV_.y = gameViewMousePosition_.y / imageSizeImGui.y;
    } else {
        // Game View 縺ｮ螟悶〒縺ｯ繝槭え繧ｹ髢｢騾｣縺ｮ蛟､繧偵Μ繧ｻ繝・ヨ縺吶ｋ
        gameViewMousePosition_ = { 0.0f, 0.0f };
        gameViewMouseUV_ = { 0.0f, 0.0f };
    }

    // Game View 上で右クリックを始めた瞬間だけ回転開始フラグを立てる
    if (isGameViewHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        isGameViewRotationStarted_ = true;
    }

    // 右ボタンを離したら回転終了にする
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        isGameViewRotationStarted_ = false;
    }

    // 回転開始後は、右ボタンを離すまで回転状態を維持する
    isGameViewRotating_ = isGameViewRotationStarted_;

    if (isGameViewRotating_) {
        // Game View 内でのマウス移動量を更新する
        gameViewMouseDelta_.x = gameViewMousePosition_.x - prevGameViewMousePosition_.x;
        gameViewMouseDelta_.y = gameViewMousePosition_.y - prevGameViewMousePosition_.y;
    } else {
        // 回転していない時は移動量を 0 に戻す
        gameViewMouseDelta_ = { 0.0f, 0.0f };
    }

    ImGui::End();
#endif
}
