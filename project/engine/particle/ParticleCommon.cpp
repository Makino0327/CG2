#include "ParticleCommon.h"

void ParticleCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRootSignature();
    CreateGraphicsPipelineState();
    CreateInitializeParticleComputeRootSignature();
    CreateInitializeParticleComputePipelineState();
    CreateEmitParticleComputeRootSignature();
    CreateEmitParticleComputePipelineState();
    CreateUpdateParticleComputeRootSignature();
    CreateUpdateParticleComputePipelineState();

}

void ParticleCommon::CommonDrawSetting()
{
    auto* commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ★ SrvManager のヒープをセット
    ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
}



void ParticleCommon::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE ranges[2]{};

    // t0 = Instancing StructuredBuffer
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].NumDescriptors = 1;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t1 = Texture2D
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].BaseShaderRegister = 1;
    ranges[1].NumDescriptors = 1;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[3]{};


    // param0 → t0 StructuredBuffer
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
    params[0].DescriptorTable.NumDescriptorRanges = 1;

    // param1 → t1 Texture2D
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
    params[1].DescriptorTable.NumDescriptorRanges = 1;

    // param2 : b0 PerView
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[2].Descriptor.ShaderRegister = 0;

    // Sampler（s0）
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ShaderRegister = 0; // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = params;
    desc.NumParameters = _countof(params);
    desc.pStaticSamplers = &sampler;
    desc.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);

    if (FAILED(hr)) {
        OutputDebugStringA((char*)err->GetBufferPointer());
        assert(false);
    }

    hr = device->CreateRootSignature(
        0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}



void ParticleCommon::CreateGraphicsPipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    // --- インプットレイアウト（3Dモデル用） ---
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        // POSITION
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

          // TEXCOORD
          { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

            // NORMAL
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
              D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    D3D12_INPUT_LAYOUT_DESC inputLayout{};
    inputLayout.pInputElementDescs = inputElementDescs;
    inputLayout.NumElements = _countof(inputElementDescs);

    // --- ブレンド ---
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    // --- ラスタライザ ---
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // ←3D は裏面カリング ON
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // --- シェーダをロード ---
    // ParticleCommon.cpp
    auto vs = dxCommon_->CompileShader(L"Resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"Resources/shaders/Particle.PS.hlsl", L"ps_6_0");


    // --- PSO 設定 ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = inputLayout;
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.BlendState = blendDesc;
    desc.RasterizerState = rasterizerDesc;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // --- Depth ---
    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = true;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.DepthStencilState = depthDesc;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // // DepthBufferの形式とPSOのDSV形式を合わせる

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;  // = 0xFFFFFFFF

    desc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

// Particle 初期化用 ComputeShader の設定を commandList に入れる
void ParticleCommon::InitializeParticleComputeSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetComputeRootSignature(initializeParticleComputeRootSignature_.Get());
    commandList->SetPipelineState(initializeParticleComputePipelineState_.Get());

    // UAV を使うので DescriptorHeap を設定する
    ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
}

void ParticleCommon::CreateInitializeParticleComputeRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE particleRange{};
    particleRange.BaseShaderRegister = 0;
    particleRange.NumDescriptors = 1;
    particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexRange{};
    freeListIndexRange.BaseShaderRegister = 1;
    freeListIndexRange.NumDescriptors = 1;
    freeListIndexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListRange{};
    freeListRange.BaseShaderRegister = 2;
    freeListRange.NumDescriptors = 1;
    freeListRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3]{};

    // u0 : Particle
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &particleRange;

    // u1 : FreeListIndex
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &freeListIndexRange;

    // u2 : FreeList
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListRange;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(initializeParticleComputeRootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}


// Particle 初期化用 ComputeShader の PipelineState を作る
void ParticleCommon::CreateInitializeParticleComputePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    auto cs = dxCommon_->CompileShader(
        L"Resources/shaders/InitializeParticle.CS.hlsl",
        L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = initializeParticleComputeRootSignature_.Get();
    desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

    HRESULT hr = device->CreateComputePipelineState(
        &desc,
        IID_PPV_ARGS(initializeParticleComputePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

// 毎フレームのParticle発生用ComputeShaderを設定する
void ParticleCommon::InitializeEmitParticleComputeSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetComputeRootSignature(emitParticleComputeRootSignature_.Get());
    commandList->SetPipelineState(emitParticleComputePipelineState_.Get());

    // UAVを使うのでDescriptorHeapも設定する
    ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
}

// Particle発生用ComputeShaderのRootSignatureを作る
void ParticleCommon::CreateEmitParticleComputeRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE particleRange{};
    particleRange.BaseShaderRegister = 0;
    particleRange.NumDescriptors = 1;
    particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexRange{};
    freeListIndexRange.BaseShaderRegister = 1;
    freeListIndexRange.NumDescriptors = 1;
    freeListIndexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListRange{};
    freeListRange.BaseShaderRegister = 2;
    freeListRange.NumDescriptors = 1;
    freeListRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[5]{};

    // b0 : Emitter
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // b1 : PerFrame
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    // u0 : Particle
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &particleRange;

    // u1 : FreeListIndex
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &freeListIndexRange;

    // u2 : FreeList
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].DescriptorTable.pDescriptorRanges = &freeListRange;


    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(emitParticleComputeRootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

// Particle発生用ComputeShaderのPipelineStateを作る
void ParticleCommon::CreateEmitParticleComputePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    auto cs = dxCommon_->CompileShader(
        L"Resources/shaders/EmitParticle.CS.hlsl",
        L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = emitParticleComputeRootSignature_.Get();
    desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

    HRESULT hr = device->CreateComputePipelineState(
        &desc,
        IID_PPV_ARGS(emitParticleComputePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

// 毎フレームのParticle更新用ComputeShaderを設定する
void ParticleCommon::InitializeUpdateParticleComputeSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetComputeRootSignature(updateParticleComputeRootSignature_.Get());
    commandList->SetPipelineState(updateParticleComputePipelineState_.Get());

    // UAVを使うのでDescriptorHeapも設定する
    ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
}

void ParticleCommon::CreateUpdateParticleComputeRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE particleRange{};
    particleRange.BaseShaderRegister = 0;
    particleRange.NumDescriptors = 1;
    particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListIndexRange{};
    freeListIndexRange.BaseShaderRegister = 1;
    freeListIndexRange.NumDescriptors = 1;
    freeListIndexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListIndexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE freeListRange{};
    freeListRange.BaseShaderRegister = 2;
    freeListRange.NumDescriptors = 1;
    freeListRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    freeListRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4]{};

    // b0 : PerFrame
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // u0 : Particle
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &particleRange;

    // u1 : FreeListIndex
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &freeListIndexRange;

    // u2 : FreeList
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &freeListRange;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(updateParticleComputeRootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

// Particle更新用ComputeShaderのPipelineStateを作る
void ParticleCommon::CreateUpdateParticleComputePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    auto cs = dxCommon_->CompileShader(
        L"Resources/shaders/UpdateParticle.CS.hlsl",
        L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = updateParticleComputeRootSignature_.Get();
    desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

    HRESULT hr = device->CreateComputePipelineState(
        &desc,
        IID_PPV_ARGS(updateParticleComputePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}
