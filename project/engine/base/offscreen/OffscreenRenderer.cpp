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

    depthSrvIndex_ = srvManager_->Allocate(); // DepthTexture用のSRV番号を確保する
    srvManager_->CreateSRVForDepthTexture(
        depthSrvIndex_,
        dxCommon_->GetDepthStencilResource()); // DepthBufferをPixelShaderから読めるようにする

    CreateRootSignature();
    CreateGraphicsPipelineState();

    // ラジアルブラー用の定数バッファを作成する
    radialBlurResource_ = dxCommon_->CreateBufferResource(sizeof(RadialBlurData));
    radialBlurResource_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurData_));

    // 初期値として画面中央を中心にして弱めにぼかす
    radialBlurData_->center = { 0.5f, 0.5f };
    radialBlurData_->blurWidth = 0.01f;
    radialBlurData_->padding = 0.0f;

    TextureManager* textureManager = TextureManager::GetInstance();

    // ディゾルブ用のマスクテクスチャを読み込む
    textureManager->LoadTexture("Resources/noise0.png");
    textureManager->LoadTexture("Resources/noise1.png");

    dissolveMaskSrvIndex0_ = textureManager->GetSrvIndex("Resources/noise0.png");
    dissolveMaskSrvIndex1_ = textureManager->GetSrvIndex("Resources/noise1.png");

    // ディゾルブ用の定数バッファを作成する
    dissolveResource_ = dxCommon_->CreateBufferResource(sizeof(DissolveData));
    dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));

    dissolveData_->threshold = 0.0f;
    dissolveData_->edgeWidth = 0.03f;
    dissolveData_->padding = { 0.0f, 0.0f };
    dissolveData_->edgeColor = { 1.0f, 0.4f, 0.3f, 1.0f };

    // ランダムノイズ用の定数バッファを作成する
    randomNoiseResource_ = dxCommon_->CreateBufferResource(sizeof(RandomNoiseData));
    randomNoiseResource_->Map(0, nullptr, reinterpret_cast<void**>(&randomNoiseData_));

    randomNoiseData_->intensity = 0.3f;
    randomNoiseData_->time = 0.0f;
    randomNoiseData_->speed = 1.0f;
    randomNoiseData_->padding = 0.0f;


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

    case PostEffectType::RadialBlur:
        // ラジアルブラー用のパイプラインステートを設定する
        commandList->SetPipelineState(radialBlurPipelineState_.Get());
        break;
    case PostEffectType::Dissolve:
        // ディゾルブ用のパイプラインステートを設定する
        commandList->SetPipelineState(dissolvePipelineState_.Get());
        break;
    case PostEffectType::RandomNoise:
        // ランダムノイズ用のパイプラインステートを設定する
        commandList->SetPipelineState(randomNoisePipelineState_.Get());
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

    D3D12_DESCRIPTOR_RANGE maskRange{};
    maskRange.BaseShaderRegister = 2;
    maskRange.NumDescriptors = 1;
    maskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    maskRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6]{};
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
    auto radialBlurPixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/RadialBlur.PS.hlsl",
        L"ps_6_0");
    auto dissolvePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Dissolve.PS.hlsl",
        L"ps_6_0");
    auto randomNoisePixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/RandomNoise.PS.hlsl",
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

    // ラジアルブラー用のピクセルシェーダを設定する
    desc.PS = {
        radialBlurPixelShaderBlob->GetBufferPointer(),
        radialBlurPixelShaderBlob->GetBufferSize()
    };

    // ラジアルブラー用のパイプラインステートを作成する
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(radialBlurPipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // ディゾルブ用のピクセルシェーダを設定する
    desc.PS = {
        dissolvePixelShaderBlob->GetBufferPointer(),
        dissolvePixelShaderBlob->GetBufferSize()
    };

    // ディゾルブ用のパイプラインステートを作成する
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(dissolvePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // ランダムノイズ用のピクセルシェーダを設定する
    desc.PS = {
        randomNoisePixelShaderBlob->GetBufferPointer(),
        randomNoisePixelShaderBlob->GetBufferSize()
    };

    // ランダムノイズ用のパイプラインステートを作成する
    hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(randomNoisePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

}
void OffscreenRenderer::Update(float deltaTime, const Vector2& mousePosition, bool isMouseRightPressed)
{
    // ランダムノイズの時間を進める
    if (randomNoiseData_) {
        randomNoiseData_->time += deltaTime * randomNoiseData_->speed;
    }

    // Game View の矩形内にマウスがあるか判定する
    const float left = gameViewTopLeft_.x;
    const float top = gameViewTopLeft_.y;
    const float right = gameViewTopLeft_.x + gameViewSize_.x;
    const float bottom = gameViewTopLeft_.y + gameViewSize_.y;

    isGameViewHovered_ =
        mousePosition.x >= left &&
        mousePosition.x <= right &&
        mousePosition.y >= top &&
        mousePosition.y <= bottom &&
        gameViewSize_.x > 0.0f &&
        gameViewSize_.y > 0.0f;

    // Game View 内ローカル座標を更新する
    prevGameViewMousePosition_ = gameViewMousePosition_;

    if (isGameViewHovered_) {
        gameViewMousePosition_.x = mousePosition.x - gameViewTopLeft_.x;
        gameViewMousePosition_.y = mousePosition.y - gameViewTopLeft_.y;

        // 0.0f ～ 1.0f の範囲に正規化する
        gameViewMouseUV_.x = gameViewMousePosition_.x / gameViewSize_.x;
        gameViewMouseUV_.y = gameViewMousePosition_.y / gameViewSize_.y;
    } else {
        gameViewMousePosition_ = { 0.0f, 0.0f };
        gameViewMouseUV_ = { 0.0f, 0.0f };
    }

    // 右クリック中だけ回転状態にする
    isGameViewRotating_ = isGameViewHovered_ && isMouseRightPressed;

    if (isGameViewRotating_) {
        gameViewMouseDelta_.x = gameViewMousePosition_.x - prevGameViewMousePosition_.x;
        gameViewMouseDelta_.y = gameViewMousePosition_.y - prevGameViewMousePosition_.y;
    } else {
        gameViewMouseDelta_ = { 0.0f, 0.0f };
    }

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

void OffscreenRenderer::SetDissolveElapsedTime(float seconds)
{
    float duration = std::max(dissolveDuration_, 0.1f);

    // 現在秒数が 0 未満や最大秒数超えにならないようにする
    dissolveElapsedTime_ = std::clamp(seconds, 0.0f, duration);

    // 現在秒数に合わせて threshold も更新する
    dissolveData_->threshold = dissolveElapsedTime_ / duration;

    // 最後まで行っていれば停止、それ以外は再生中扱いにする
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

        // Game View の表示位置とサイズだけを保存しておく
    ImVec2 imageMin = ImGui::GetItemRectMin();
    ImVec2 imageSizeImGui = ImGui::GetItemRectSize();

    gameViewTopLeft_ = { imageMin.x, imageMin.y };
    gameViewSize_ = { imageSizeImGui.x, imageSizeImGui.y };

    ImGui::End();
#endif
}
